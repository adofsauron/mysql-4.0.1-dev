/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
  This will make mf_rec_cache obsolete.
  One can change info->pos_in_file to a higher value to skip bytes in file if
  also info->rc_pos is set to info->rc_end.
  If called through open_cached_file(), then the temporary file will
  only be created if a write exeeds the file buffer or if one calls
  flush_io_cache().  
*/

#define MAP_TO_USE_RAID
#include "mysql_priv.h"
#ifdef HAVE_AIOWAIT
#include <mysys_err.h>
#include <errno.h>
static void my_aiowait(my_aio_result *result);
#endif
#include <assert.h>

extern "C" {

	/*
	** Read buffered from the net.
	** Returns 1 if can't read requested characters
	** Returns 0 if record read
	*/

int _my_b_net_read(register IO_CACHE *info, byte *Buffer,
		   uint Count __attribute__((unused)))
{
  ulong read_length;
  NET *net= &(current_thd)->net;
  DBUG_ENTER("_my_b_net_read");

  if (!info->end_of_file)
    DBUG_RETURN(1);	/* because my_b_get (no _) takes 1 byte at a time */
  read_length=my_net_read(net);
  if (read_length == packet_error)
  {
    info->error= -1;
    DBUG_RETURN(1);
  }
  if (read_length == 0)
  {
    info->end_of_file= 0;			/* End of file from client */
    DBUG_RETURN(1);
  }
  /* to set up stuff for my_b_get (no _) */
  info->read_end = (info->read_pos = (byte*) net->read_pos) + read_length;
  Buffer[0] = info->read_pos[0];		/* length is always 1 */
  info->read_pos++;

  /*
    info->request_pos is used by log_loaded_block() to know the size
    of the current block
  */
  info->request_pos=info->read_pos;
  DBUG_RETURN(0);
}

} /* extern "C" */
