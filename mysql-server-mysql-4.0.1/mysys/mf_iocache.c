/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Cashing of files with only does (sequential) read or writes of fixed-
  length records. A read isn't allowed to go over file-length. A read is ok
  if it ends at file-length and next read can try to read after file-length
  (and get a EOF-error).
  Possibly use of asyncronic io.
  macros for read and writes for faster io.
  Used instead of FILE when reading or writing whole files.
  This code makes mf_rec_cache obsolete (currently only used by ISAM)
  One can change info->pos_in_file to a higher value to skip bytes in file if
  also info->read_pos is set to info->read_end.
  If called through open_cached_file(), then the temporary file will
  only be created if a write exeeds the file buffer or if one calls
  flush_io_cache().

  If one uses SEQ_READ_APPEND, then two buffers are allocated, one for
  reading and another for writing.  Reads are first done from disk and
  then done from the write buffer.  This is an efficient way to read
  from a log file when one is writing to it at the same time.
  For this to work, the file has to be opened in append mode!
  Note that when one uses SEQ_READ_APPEND, one MUST write using
  my_b_append !  This is needed because we need to lock the mutex
  every time we access the write buffer.

TODO:
  When one SEQ_READ_APPEND and we are reading and writing at the same time,
  each time the write buffer gets full and it's written to disk, we will
  always do a disk read to read a part of the buffer from disk to the
  read buffer.
  This should be fixed so that when we do a flush_io_cache() and
  we have been reading the write buffer, we should transfer the rest of the
  write buffer to the read buffer before we start to reuse it.
*/

#define MAP_TO_USE_RAID
#include "mysys_priv.h"
#include <m_string.h>
#ifdef HAVE_AIOWAIT
#include "mysys_err.h"
static void my_aiowait(my_aio_result *result);
#endif
#include <assert.h>
#include <errno.h>

#ifdef THREAD
#define lock_append_buffer(info) \
 pthread_mutex_lock(&(info)->append_buffer_lock)
#define unlock_append_buffer(info) \
 pthread_mutex_unlock(&(info)->append_buffer_lock)
#else
#define lock_append_buffer(info)
#define unlock_append_buffer(info)
#endif

static void
init_functions(IO_CACHE* info, enum cache_type type)
{
  switch (type) {
  case READ_NET:
    /* must be initialized by the caller. The problem is that
       _my_b_net_read has to be defined in sql directory because of
       the dependency on THD, and therefore cannot be visible to
       programs that link against mysys but know nothing about THD, such
       as myisamchk
    */
    break;
  case SEQ_READ_APPEND:
    info->read_function = _my_b_seq_read;
    info->write_function = 0;			/* Force a core if used */
    break;
  default:
    info->read_function = _my_b_read;
    info->write_function = _my_b_write;
  }

  /* Ensure that my_b_tell() and my_b_bytes_in_cache works */
  if (type == WRITE_CACHE)
  {
    info->current_pos= &info->write_pos;
    info->current_end= &info->write_end;
  }
  else
  {
    info->current_pos= &info->read_pos;
    info->current_end= &info->read_end;
  }
}

	/*
	** if cachesize == 0 then use default cachesize (from s-file)
	** if file == -1 then real_open_cached_file() will be called.
	** returns 0 if ok
	*/

int init_io_cache(IO_CACHE *info, File file, uint cachesize,
		  enum cache_type type, my_off_t seek_offset,
		  pbool use_async_io, myf cache_myflags)
{
  uint min_cache;
  my_off_t end_of_file= ~(my_off_t) 0;
  DBUG_ENTER("init_io_cache");
  DBUG_PRINT("enter",("type: %d  pos: %ld",(int) type, (ulong) seek_offset));

  info->file= file;
  info->type=type;
  info->pos_in_file= seek_offset;
  info->pre_close = info->pre_read = info->post_read = 0;
  info->arg = 0;
  info->alloced_buffer = 0;
  info->buffer=0;
  info->seek_not_done= test(file >= 0);

  if (!cachesize)
    if (! (cachesize= my_default_record_cache_size))
      DBUG_RETURN(1);				/* No cache requested */
  min_cache=use_async_io ? IO_SIZE*4 : IO_SIZE*2;
  if (type == READ_CACHE || type == SEQ_READ_APPEND)
  {						/* Assume file isn't growing */
    if (!(cache_myflags & MY_DONT_CHECK_FILESIZE))
    {
      /* Calculate end of file to not allocate to big buffers */
      end_of_file=my_seek(file,0L,MY_SEEK_END,MYF(0));
      if (end_of_file < seek_offset)
	end_of_file=seek_offset;
      /* Trim cache size if the file is very small */
      if ((my_off_t) cachesize > end_of_file-seek_offset+IO_SIZE*2-1)
      {
	cachesize=(uint) (end_of_file-seek_offset)+IO_SIZE*2-1;
	use_async_io=0;				/* No need to use async */
      }
    }
  }
  cache_myflags &= ~MY_DONT_CHECK_FILESIZE;
  if (type != READ_NET && type != WRITE_NET)
  {
    /* Retry allocating memory in smaller blocks until we get one */
    for (;;)
    {
      uint buffer_block;
      cachesize=(uint) ((ulong) (cachesize + min_cache-1) &
			(ulong) ~(min_cache-1));
      if (cachesize < min_cache)
	cachesize = min_cache;
      buffer_block = cachesize;
      if (type == SEQ_READ_APPEND)
	buffer_block *= 2;
      if ((info->buffer=
	   (byte*) my_malloc(buffer_block,
			     MYF((cache_myflags & ~ MY_WME) |
				 (cachesize == min_cache ? MY_WME : 0)))) != 0)
      {
	info->write_buffer=info->buffer;
	if (type == SEQ_READ_APPEND)
	  info->write_buffer = info->buffer + cachesize;
	info->alloced_buffer=1;
	break;					/* Enough memory found */
      }
      if (cachesize == min_cache)
	DBUG_RETURN(2);				/* Can't alloc cache */
      cachesize= (uint) ((long) cachesize*3/4); /* Try with less memory */
    }
  }

  DBUG_PRINT("info",("init_io_cache: cachesize = %u",cachesize));
  info->read_length=info->buffer_length=cachesize;
  info->myflags=cache_myflags & ~(MY_NABP | MY_FNABP);
  info->request_pos= info->read_pos= info->write_pos = info->buffer;
  if (type == SEQ_READ_APPEND)
  {
    info->append_read_pos = info->write_pos = info->write_buffer;
    info->write_end = info->write_buffer + info->buffer_length;
#ifdef THREAD
    pthread_mutex_init(&info->append_buffer_lock,MY_MUTEX_INIT_FAST);
#endif
  }

  if (type == WRITE_CACHE)
    info->write_end=
      info->buffer+info->buffer_length- (seek_offset & (IO_SIZE-1));
  else
    info->read_end=info->buffer;		/* Nothing in cache */

  /* End_of_file may be changed by user later */
  info->end_of_file= end_of_file;
  info->error=0;
  init_functions(info,type);
#ifdef HAVE_AIOWAIT
  if (use_async_io && ! my_disable_async_io)
  {
    DBUG_PRINT("info",("Using async io"));
    info->read_length/=2;
    info->read_function=_my_b_async_read;
  }
  info->inited=info->aio_result.pending=0;
#endif
  DBUG_RETURN(0);
}						/* init_io_cache */


	/* Wait until current request is ready */

#ifdef HAVE_AIOWAIT
static void my_aiowait(my_aio_result *result)
{
  if (result->pending)
  {
    struct aio_result_t *tmp;
    for (;;)
    {
      if ((int) (tmp=aiowait((struct timeval *) 0)) == -1)
      {
	if (errno == EINTR)
	  continue;
	DBUG_PRINT("error",("No aio request, error: %d",errno));
	result->pending=0;			/* Assume everythings is ok */
	break;
      }
      ((my_aio_result*) tmp)->pending=0;
      if ((my_aio_result*) tmp == result)
	break;
    }
  }
  return;
}
#endif


/*
  Use this to reset cache to re-start reading or to change the type
  between READ_CACHE <-> WRITE_CACHE
  If we are doing a reinit of a cache where we have the start of the file
  in the cache, we are reusing this memory without flushing it to disk.
*/

my_bool reinit_io_cache(IO_CACHE *info, enum cache_type type,
			my_off_t seek_offset,
			pbool use_async_io __attribute__((unused)),
			pbool clear_cache)
{
  DBUG_ENTER("reinit_io_cache");
  DBUG_PRINT("enter",("type: %d  seek_offset: %lu  clear_cache: %d",
		      type, (ulong) seek_offset, (int) clear_cache));

  /* One can't do reinit with the following types */
  DBUG_ASSERT(type != READ_NET && info->type != READ_NET &&
	      type != WRITE_NET && info->type != WRITE_NET &&
	      type != SEQ_READ_APPEND && info->type != SEQ_READ_APPEND);

  /* If the whole file is in memory, avoid flushing to disk */
  if (! clear_cache &&
      seek_offset >= info->pos_in_file &&
      seek_offset <= my_b_tell(info))
  {
    /* Reuse current buffer without flushing it to disk */
    byte *pos;
    if (info->type == WRITE_CACHE && type == READ_CACHE)
    {
      info->read_end=info->write_pos;
      info->end_of_file=my_b_tell(info);
    }
    else if (type == WRITE_CACHE)
    {
      if (info->type == READ_CACHE)
	info->write_end=info->write_buffer+info->buffer_length;
      info->end_of_file = ~(my_off_t) 0;
    }
    pos=info->request_pos+(seek_offset-info->pos_in_file);
    if (type == WRITE_CACHE)
      info->write_pos=pos;
    else
      info->read_pos= pos;
#ifdef HAVE_AIOWAIT
    my_aiowait(&info->aio_result);		/* Wait for outstanding req */
#endif
  }
  else
  {
    /*
      If we change from WRITE_CACHE to READ_CACHE, assume that everything
      after the current positions should be ignored
    */
    if (info->type == WRITE_CACHE && type == READ_CACHE)
      info->end_of_file=my_b_tell(info);
    /* flush cache if we want to reuse it */
    if (!clear_cache && flush_io_cache(info))
      DBUG_RETURN(1);
    info->pos_in_file=seek_offset;
    /* Better to do always do a seek */
    info->seek_not_done=1;
    info->request_pos=info->read_pos=info->write_pos=info->buffer;
    if (type == READ_CACHE)
    {
      info->read_end=info->buffer;		/* Nothing in cache */
    }
    else
    {
      info->write_end=(info->buffer + info->buffer_length -
		       (seek_offset & (IO_SIZE-1)));
      info->end_of_file= ~(my_off_t) 0;
    }
  }
  info->type=type;
  info->error=0;
  init_functions(info,type);

#ifdef HAVE_AIOWAIT
  if (use_async_io && ! my_disable_async_io &&
      ((ulong) info->buffer_length <
       (ulong) (info->end_of_file - seek_offset)))
  {
    info->read_length=info->buffer_length/2;
    info->read_function=_my_b_async_read;
  }
  info->inited=0;
#endif
  DBUG_RETURN(0);
} /* reinit_io_cache */



/*
  Read buffered. Returns 1 if can't read requested characters
  This function is only called from the my_b_read() macro
  when there isn't enough characters in the buffer to
  satisfy the request.
  Returns 0 we succeeded in reading all data
*/

int _my_b_read(register IO_CACHE *info, byte *Buffer, uint Count)
{
  uint length,diff_length,left_length;
  my_off_t max_length, pos_in_file;
  DBUG_ENTER("_my_b_read");

  if ((left_length=(uint) (info->read_end-info->read_pos)))
  {
    DBUG_ASSERT(Count >= left_length);	/* User is not using my_b_read() */
    memcpy(Buffer,info->read_pos, (size_t) (left_length));
    Buffer+=left_length;
    Count-=left_length;
  }

  /* pos_in_file always point on where info->buffer was read */
  pos_in_file=info->pos_in_file+(uint) (info->read_end - info->buffer);
  if (info->seek_not_done)
  {					/* File touched, do seek */
    VOID(my_seek(info->file,pos_in_file,MY_SEEK_SET,MYF(0)));
    info->seek_not_done=0;
  }
  diff_length=(uint) (pos_in_file & (IO_SIZE-1));
  if (Count >= (uint) (IO_SIZE+(IO_SIZE-diff_length)))
  {					/* Fill first intern buffer */
    uint read_length;
    if (info->end_of_file == pos_in_file)
    {					/* End of file */
      info->error=(int) left_length;
      DBUG_RETURN(1);
    }
    length=(Count & (uint) ~(IO_SIZE-1))-diff_length;
    if ((read_length=my_read(info->file,Buffer,(uint) length,info->myflags))
	!= (uint) length)
    {
      info->error= read_length == (uint) -1 ? -1 :
	(int) (read_length+left_length);
      DBUG_RETURN(1);
    }
    Count-=length;
    Buffer+=length;
    pos_in_file+=length;
    left_length+=length;
    diff_length=0;
  }

  max_length=info->read_length-diff_length;
  if (info->type != READ_FIFO &&
      max_length > (info->end_of_file - pos_in_file))
    max_length = info->end_of_file - pos_in_file;
  if (!max_length)
  {
    if (Count)
    {
      info->error= left_length;		/* We only got this many char */
      DBUG_RETURN(1);
    }
    length=0;				/* Didn't read any chars */
  }
  else if ((length=my_read(info->file,info->buffer,(uint) max_length,
			   info->myflags)) < Count ||
	   length == (uint) -1)
  {
    if (length != (uint) -1)
      memcpy(Buffer,info->buffer,(size_t) length);
    info->error= length == (uint) -1 ? -1 : (int) (length+left_length);
    info->read_pos=info->read_end=info->buffer;
    DBUG_RETURN(1);
  }
  info->read_pos=info->buffer+Count;
  info->read_end=info->buffer+length;
  info->pos_in_file=pos_in_file;
  memcpy(Buffer,info->buffer,(size_t) Count);
  DBUG_RETURN(0);
}

/*
  Do sequential read from the SEQ_READ_APPEND cache
  we do this in three stages:
   - first read from info->buffer
   - then if there are still data to read, try the file descriptor
   - afterwards, if there are still data to read, try append buffer
*/

int _my_b_seq_read(register IO_CACHE *info, byte *Buffer, uint Count)
{
  uint length,diff_length,left_length,save_count;
  my_off_t max_length, pos_in_file;
  save_count=Count;

  /* first, read the regular buffer */
  if ((left_length=(uint) (info->read_end-info->read_pos)))
  {
    DBUG_ASSERT(Count > left_length);	/* User is not using my_b_read() */
    memcpy(Buffer,info->read_pos, (size_t) (left_length));
    Buffer+=left_length;
    Count-=left_length;
  }
  lock_append_buffer(info);

  /* pos_in_file always point on where info->buffer was read */
  if ((pos_in_file=info->pos_in_file+(uint) (info->read_end - info->buffer)) >=
      info->end_of_file)
    goto read_append_buffer;

  if (info->seek_not_done)
  {					/* File touched, do seek */
    VOID(my_seek(info->file,pos_in_file,MY_SEEK_SET,MYF(0)));
    info->seek_not_done=0;
  }
  diff_length=(uint) (pos_in_file & (IO_SIZE-1));

  /* now the second stage begins - read from file descriptor */
  if (Count >= (uint) (IO_SIZE+(IO_SIZE-diff_length)))
  {					/* Fill first intern buffer */
    uint read_length;

    length=(Count & (uint) ~(IO_SIZE-1))-diff_length;
    if ((read_length=my_read(info->file,Buffer,(uint) length,info->myflags)) ==
	(uint)-1)
    {
      info->error= -1;
      unlock_append_buffer(info);
      return 1;
    }
    Count-=read_length;
    Buffer+=read_length;
    pos_in_file+=read_length;

    if (read_length != (uint) length)
    {
      /*
	We only got part of data;  Read the rest of the data from the
	write buffer
      */
      goto read_append_buffer;
    }
    left_length+=length;
    diff_length=0;
  }

  max_length=info->read_length-diff_length;
  if (max_length > (info->end_of_file - pos_in_file))
    max_length = info->end_of_file - pos_in_file;
  if (!max_length)
  {
    if (Count)
      goto read_append_buffer;
    length=0;				/* Didn't read any more chars */
  }
  else
  {
    length=my_read(info->file,info->buffer,(uint) max_length,
		   info->myflags);
    if (length == (uint) -1)
    {
      info->error= -1;
      unlock_append_buffer(info);
      return 1;
    }
    if (length < Count)
    {
      memcpy(Buffer,info->buffer,(size_t) length);
      Count -= length;
      Buffer += length;
      goto read_append_buffer;
    }
  }
  unlock_append_buffer(info);
  info->read_pos=info->buffer+Count;
  info->read_end=info->buffer+length;
  info->pos_in_file=pos_in_file;
  memcpy(Buffer,info->buffer,(size_t) Count);
  return 0;

read_append_buffer:

  /*
     Read data from the current write buffer.
     Count should never be == 0 here (The code will work even if count is 0)
  */

  {
    /* First copy the data to Count */
    uint len_in_buff = (uint) (info->write_pos - info->append_read_pos);
    uint copy_len;

    DBUG_ASSERT(info->append_read_pos <= info->write_pos);
    DBUG_ASSERT(pos_in_file == info->end_of_file);

    copy_len=min(Count, len_in_buff);
    memcpy(Buffer, info->append_read_pos, copy_len);
    info->append_read_pos += copy_len;
    Count -= copy_len;
    if (Count)
      info->error = save_count - Count;

    /* Fill read buffer with data from write buffer */
    memcpy(info->buffer, info->append_read_pos,
	   (size_t) (len_in_buff - copy_len));
    info->read_pos= info->buffer;
    info->read_end= info->buffer+(len_in_buff - copy_len);
    info->append_read_pos=info->write_pos;
    info->pos_in_file+=len_in_buff;
  }
  unlock_append_buffer(info);
  return Count ? 1 : 0;
}


#ifdef HAVE_AIOWAIT

int _my_b_async_read(register IO_CACHE *info, byte *Buffer, uint Count)
{
  uint length,read_length,diff_length,left_length,use_length,org_Count;
  my_off_t max_length;
  my_off_t next_pos_in_file;
  byte *read_buffer;

  memcpy(Buffer,info->read_pos,
	 (size_t) (left_length=(uint) (info->read_end-info->read_pos)));
  Buffer+=left_length;
  org_Count=Count;
  Count-=left_length;

  if (info->inited)
  {						/* wait for read block */
    info->inited=0;				/* No more block to read */
    my_aiowait(&info->aio_result);		/* Wait for outstanding req */
    if (info->aio_result.result.aio_errno)
    {
      if (info->myflags & MY_WME)
	my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(info->file),
		 info->aio_result.result.aio_errno);
      my_errno=info->aio_result.result.aio_errno;
      info->error= -1;
      return(1);
    }
    if (! (read_length = (uint) info->aio_result.result.aio_return) ||
	read_length == (uint) -1)
    {
      my_errno=0;				/* For testing */
      info->error= (read_length == (uint) -1 ? -1 :
		    (int) (read_length+left_length));
      return(1);
    }
    info->pos_in_file+=(uint) (info->read_end - info->request_pos);

    if (info->request_pos != info->buffer)
      info->request_pos=info->buffer;
    else
      info->request_pos=info->buffer+info->read_length;
    info->read_pos=info->request_pos;
    next_pos_in_file=info->aio_read_pos+read_length;

	/* Check if pos_in_file is changed
	   (_ni_read_cache may have skipped some bytes) */

    if (info->aio_read_pos < info->pos_in_file)
    {						/* Fix if skipped bytes */
      if (info->aio_read_pos + read_length < info->pos_in_file)
      {
	read_length=0;				/* Skipp block */
	next_pos_in_file=info->pos_in_file;
      }
      else
      {
	my_off_t offset= (info->pos_in_file - info->aio_read_pos);
	info->pos_in_file=info->aio_read_pos; /* Whe are here */
	info->read_pos=info->request_pos+offset;
	read_length-=offset;			/* Bytes left from read_pos */
      }
    }
#ifndef DBUG_OFF
    if (info->aio_read_pos > info->pos_in_file)
    {
      my_errno=EINVAL;
      return(info->read_length= -1);
    }
#endif
	/* Copy found bytes to buffer */
    length=min(Count,read_length);
    memcpy(Buffer,info->read_pos,(size_t) length);
    Buffer+=length;
    Count-=length;
    left_length+=length;
    info->read_end=info->rc_pos+read_length;
    info->read_pos+=length;
  }
  else
    next_pos_in_file=(info->pos_in_file+ (uint)
		      (info->read_end - info->request_pos));

	/* If reading large blocks, or first read or read with skipp */
  if (Count)
  {
    if (next_pos_in_file == info->end_of_file)
    {
      info->error=(int) (read_length+left_length);
      return 1;
    }
    VOID(my_seek(info->file,next_pos_in_file,MY_SEEK_SET,MYF(0)));
    read_length=IO_SIZE*2- (uint) (next_pos_in_file & (IO_SIZE-1));
    if (Count < read_length)
    {					/* Small block, read to cache */
      if ((read_length=my_read(info->file,info->request_pos,
			       read_length, info->myflags)) == (uint) -1)
	return info->error= -1;
      use_length=min(Count,read_length);
      memcpy(Buffer,info->request_pos,(size_t) use_length);
      info->read_pos=info->request_pos+Count;
      info->read_end=info->request_pos+read_length;
      info->pos_in_file=next_pos_in_file;	/* Start of block in cache */
      next_pos_in_file+=read_length;

      if (Count != use_length)
      {					/* Didn't find hole block */
	if (info->myflags & (MY_WME | MY_FAE | MY_FNABP) && Count != org_Count)
	  my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
		   my_filename(info->file),my_errno);
	info->error=(int) (read_length+left_length);
	return 1;
      }
    }
    else
    {						/* Big block, don't cache it */
      if ((read_length=my_read(info->file,Buffer,(uint) Count,info->myflags))
	  != Count)
      {
	info->error= read_length == (uint)  -1 ? -1 : read_length+left_length;
	return 1;
      }
      info->read_pos=info->read_end=info->request_pos;
      info->pos_in_file=(next_pos_in_file+=Count);
    }
  }

	/* Read next block with asyncronic io */
  max_length=info->end_of_file - next_pos_in_file;
  diff_length=(next_pos_in_file & (IO_SIZE-1));

  if (max_length > (my_off_t) info->read_length - diff_length)
    max_length= (my_off_t) info->read_length - diff_length;
  if (info->request_pos != info->buffer)
    read_buffer=info->buffer;
  else
    read_buffer=info->buffer+info->read_length;
  info->aio_read_pos=next_pos_in_file;
  if (max_length)
  {
    info->aio_result.result.aio_errno=AIO_INPROGRESS;	/* Marker for test */
    DBUG_PRINT("aioread",("filepos: %ld  length: %ld",
			  (ulong) next_pos_in_file,(ulong) max_length));
    if (aioread(info->file,read_buffer,(int) max_length,
		(my_off_t) next_pos_in_file,MY_SEEK_SET,
		&info->aio_result.result))
    {						/* Skipp async io */
      my_errno=errno;
      DBUG_PRINT("error",("got error: %d, aio_result: %d from aioread, async skipped",
			  errno, info->aio_result.result.aio_errno));
      if (info->request_pos != info->buffer)
      {
	bmove(info->buffer,info->request_pos,
	      (uint) (info->read_end - info->read_pos));
	info->request_pos=info->buffer;
	info->read_pos-=info->read_length;
	info->read_end-=info->read_length;
      }
      info->read_length=info->buffer_length;	/* Use hole buffer */
      info->read_function=_my_b_read;		/* Use normal IO_READ next */
    }
    else
      info->inited=info->aio_result.pending=1;
  }
  return 0;					/* Block read, async in use */
} /* _my_b_async_read */
#endif


/* Read one byte when buffer is empty */

int _my_b_get(IO_CACHE *info)
{
  byte buff;
  IO_CACHE_CALLBACK pre_read,post_read;
  if ((pre_read = info->pre_read))
    (*pre_read)(info);
  if ((*(info)->read_function)(info,&buff,1))
    return my_b_EOF;
  if ((post_read = info->post_read))
    (*post_read)(info);
  return (int) (uchar) buff;
}

	/* Returns != 0 if error on write */

int _my_b_write(register IO_CACHE *info, const byte *Buffer, uint Count)
{
  uint rest_length,length;

  if (info->pos_in_file+info->buffer_length > info->end_of_file)
  {
    my_errno=errno=EFBIG;
    return info->error = -1;
  }

  rest_length=(uint) (info->write_end - info->write_pos);
  memcpy(info->write_pos,Buffer,(size_t) rest_length);
  Buffer+=rest_length;
  Count-=rest_length;
  info->write_pos+=rest_length;
  if (flush_io_cache(info))
    return 1;
  if (Count >= IO_SIZE)
  {					/* Fill first intern buffer */
    length=Count & (uint) ~(IO_SIZE-1);
    if (info->seek_not_done)
    {					/* File touched, do seek */
      VOID(my_seek(info->file,info->pos_in_file,MY_SEEK_SET,MYF(0)));
      info->seek_not_done=0;
    }
    if (my_write(info->file,Buffer,(uint) length,info->myflags | MY_NABP))
      return info->error= -1;
    Count-=length;
    Buffer+=length;
    info->pos_in_file+=length;
  }
  memcpy(info->write_pos,Buffer,(size_t) Count);
  info->write_pos+=Count;
  return 0;
}


/*
  Append a block to the write buffer.
  This is done with the buffer locked to ensure that we don't read from
  the write buffer before we are ready with it.
*/

int my_b_append(register IO_CACHE *info, const byte *Buffer, uint Count)
{
  uint rest_length,length;

  lock_append_buffer(info);
  rest_length=(uint) (info->write_end - info->write_pos);
  if (Count <= rest_length)
    goto end;
  memcpy(info->write_pos,Buffer,(size_t) rest_length);
  Buffer+=rest_length;
  Count-=rest_length;
  info->write_pos+=rest_length;
  if (flush_io_cache(info))
    return 1;
  if (Count >= IO_SIZE)
  {					/* Fill first intern buffer */
    length=Count & (uint) ~(IO_SIZE-1);
    if (my_write(info->file,Buffer,(uint) length,info->myflags | MY_NABP))
      return info->error= -1;
    Count-=length;
    Buffer+=length;
  }

end:
  memcpy(info->write_pos,Buffer,(size_t) Count);
  info->write_pos+=Count;
  unlock_append_buffer(info);
  return 0;
}


/*
  Write a block to disk where part of the data may be inside the record
  buffer.  As all write calls to the data goes through the cache,
  we will never get a seek over the end of the buffer
*/

int my_block_write(register IO_CACHE *info, const byte *Buffer, uint Count,
		   my_off_t pos)
{
  uint length;
  int error=0;

  if (pos < info->pos_in_file)
  {
    /* Of no overlap, write everything without buffering */
    if (pos + Count <= info->pos_in_file)
      return my_pwrite(info->file, Buffer, Count, pos,
		       info->myflags | MY_NABP);
    /* Write the part of the block that is before buffer */
    length= (uint) (info->pos_in_file - pos);
    if (my_pwrite(info->file, Buffer, length, pos, info->myflags | MY_NABP))
      info->error=error=-1;
    Buffer+=length;
    pos+=  length;
    Count-= length;
#ifndef HAVE_PREAD
    info->seek_not_done=1;
#endif
  }

  /* Check if we want to write inside the used part of the buffer.*/
  length= (uint) (info->write_end - info->buffer);
  if (pos < info->pos_in_file + length)
  {
    uint offset= (uint) (pos - info->pos_in_file);
    length-=offset;
    if (length > Count)
      length=Count;
    memcpy(info->buffer+offset, Buffer, length);
    Buffer+=length;
    Count-= length;
    /* Fix length of buffer if the new data was larger */
    if (info->buffer+length > info->write_pos)
      info->write_pos=info->buffer+length;
    if (!Count)
      return (error);
  }
  /* Write at the end of the current buffer; This is the normal case */
  if (_my_b_write(info, Buffer, Count))
    error= -1;
  return error;
}


	/* Flush write cache */

int flush_io_cache(IO_CACHE *info)
{
  uint length;
  my_bool append_cache;
  my_off_t pos_in_file;
  DBUG_ENTER("flush_io_cache");

  append_cache = (info->type == SEQ_READ_APPEND);
  if (info->type == WRITE_CACHE || append_cache)
  {
    if (info->file == -1)
    {
      if (real_open_cached_file(info))
	DBUG_RETURN((info->error= -1));
    }
    if ((length=(uint) (info->write_pos - info->write_buffer)))
    {
      pos_in_file=info->pos_in_file;
      if (append_cache)
      {
	pos_in_file=info->end_of_file;
	info->seek_not_done=1;
      }
      if (info->seek_not_done)
      {					/* File touched, do seek */
	if (my_seek(info->file,pos_in_file,MY_SEEK_SET,MYF(0)) ==
	    MY_FILEPOS_ERROR)
	{
	  DBUG_RETURN((info->error= -1));
	}
	if (!append_cache)
	  info->seek_not_done=0;
      }
      info->write_pos= info->write_buffer;
      if (!append_cache)
	info->pos_in_file+=length;
      info->write_end= (info->write_buffer+info->buffer_length-
			((pos_in_file+length) & (IO_SIZE-1)));

      /* Set this to be used if we are using SEQ_READ_APPEND */
      info->append_read_pos = info->write_buffer;
      if (my_write(info->file,info->write_buffer,length,
		   info->myflags | MY_NABP))
	info->error= -1;
      else
	info->error= 0;
      set_if_bigger(info->end_of_file,(pos_in_file+length));
      DBUG_RETURN(info->error);
    }
  }
#ifdef HAVE_AIOWAIT
  else if (info->type != READ_NET)
  {
    my_aiowait(&info->aio_result);		/* Wait for outstanding req */
    info->inited=0;
  }
#endif
  DBUG_RETURN(0);
}


int end_io_cache(IO_CACHE *info)
{
  int error=0;
  IO_CACHE_CALLBACK pre_close;
  DBUG_ENTER("end_io_cache");

  if ((pre_close=info->pre_close))
    (*pre_close)(info);
  if (info->alloced_buffer)
  {
    info->alloced_buffer=0;
    if (info->file != -1)			/* File doesn't exist */
      error=flush_io_cache(info);
    my_free((gptr) info->buffer,MYF(MY_WME));
    info->buffer=info->read_pos=(byte*) 0;
  }
  if (info->type == SEQ_READ_APPEND)
  {
    /* Destroy allocated mutex */
    info->type=0;
#ifdef THREAD
    pthread_mutex_destroy(&info->append_buffer_lock);
#endif
  }
  DBUG_RETURN(error);
} /* end_io_cache */


/**********************************************************************
 Testing of MF_IOCACHE
**********************************************************************/

#ifdef MAIN

#include <my_dir.h>

void die(const char* fmt, ...)
{
  va_list va_args;
  va_start(va_args,fmt);
  fprintf(stderr,"Error:");
  vfprintf(stderr, fmt,va_args);
  fprintf(stderr,", errno=%d\n", errno);
  exit(1);
}

int open_file(const char* fname, IO_CACHE* info, int cache_size)
{
  int fd;
  if ((fd=my_open(fname,O_CREAT | O_RDWR,MYF(MY_WME))) < 0)
    die("Could not open %s", fname);
  if (init_io_cache(info, fd, cache_size, SEQ_READ_APPEND, 0,0,MYF(MY_WME)))
    die("failed in init_io_cache()");
  return fd;
}

void close_file(IO_CACHE* info)
{
  end_io_cache(info);
  my_close(info->file, MYF(MY_WME));
}

int main(int argc, char** argv)
{
  IO_CACHE sra_cache; /* SEQ_READ_APPEND */
  MY_STAT status;
  const char* fname="/tmp/iocache.test";
  int cache_size=16384;
  char llstr_buf[22];
  int max_block,total_bytes=0;
  int i,num_loops=100,error=0;
  char *p;
  char* block, *block_end;
  MY_INIT(argv[0]);
  max_block = cache_size*3;
  if (!(block=(char*)my_malloc(max_block,MYF(MY_WME))))
    die("Not enough memory to allocate test block");
  block_end = block + max_block;
  for (p = block,i=0; p < block_end;i++)
  {
    *p++ = (char)i;
  }
  if (my_stat(fname,&status, MYF(0)) &&
      my_delete(fname,MYF(MY_WME)))
    {
      die("Delete of %s failed, aborting", fname);
    }
  open_file(fname,&sra_cache, cache_size);
  for (i = 0; i < num_loops; i++)
  {
    char buf[4];
    int block_size = abs(rand() % max_block);
    int4store(buf, block_size);
    if (my_b_append(&sra_cache,buf,4) ||
	my_b_append(&sra_cache, block, block_size))
      die("write failed");
    total_bytes += 4+block_size;
  }
  close_file(&sra_cache);
  my_free(block,MYF(MY_WME));
  if (!my_stat(fname,&status,MYF(MY_WME)))
    die("%s failed to stat, but I had just closed it,\
 wonder how that happened");
  printf("Final size of %s is %s, wrote %d bytes\n",fname,
	 llstr(status.st_size,llstr_buf),
	 total_bytes);
  my_delete(fname, MYF(MY_WME));
  /* check correctness of tests */
  if (total_bytes != status.st_size)
  {
    fprintf(stderr,"Not the same number of bytes acutally  in file as bytes \
supposedly written\n");
    error=1;
  }
  exit(error);
  return 0;
}
#endif
