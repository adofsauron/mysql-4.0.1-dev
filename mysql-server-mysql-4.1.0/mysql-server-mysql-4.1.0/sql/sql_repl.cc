/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB & Sasha

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

// Sasha Pachev <sasha@mysql.com> is currently in charge of this file

#include "mysql_priv.h"
#ifdef HAVE_REPLICATION

#include "sql_repl.h"
#include "sql_acl.h"
#include "log_event.h"
#include "mini_client.h"
#include <my_dir.h>

extern const char* any_db;

int max_binlog_dump_events = 0; // unlimited
my_bool opt_sporadic_binlog_dump_fail = 0;
static int binlog_dump_count = 0;

int check_binlog_magic(IO_CACHE* log, const char** errmsg)
{
  char magic[4];
  DBUG_ASSERT(my_b_tell(log) == 0);

  if (my_b_read(log, (byte*) magic, sizeof(magic)))
  {
    *errmsg = "I/O error reading the header from the binary log";
    sql_print_error("%s, errno=%d, io cache code=%d", *errmsg, my_errno,
		    log->error);
    return 1;
  }
  if (memcmp(magic, BINLOG_MAGIC, sizeof(magic)))
  {
    *errmsg = "Binlog has bad magic number;  It's not a binary log file that can be used by this version of MySQL";
    return 1;
  }
  return 0;
}

static int fake_rotate_event(NET* net, String* packet, char* log_file_name,
			     const char**errmsg)
{
  char header[LOG_EVENT_HEADER_LEN], buf[ROTATE_HEADER_LEN];
  memset(header, 0, 4); // when does not matter
  header[EVENT_TYPE_OFFSET] = ROTATE_EVENT;

  char* p = log_file_name+dirname_length(log_file_name);
  uint ident_len = (uint) strlen(p);
  ulong event_len = ident_len + ROTATE_EVENT_OVERHEAD;
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int2store(header + FLAGS_OFFSET, 0);
  
  // TODO: check what problems this may cause and fix them
  int4store(header + LOG_POS_OFFSET, 0);
  
  packet->append(header, sizeof(header));
  /* We need to split the next statement because of problem with cxx */
  int4store(buf,4); // tell slave to skip magic number
  int4store(buf+4,0);
  packet->append(buf, ROTATE_HEADER_LEN);
  packet->append(p,ident_len);
  if (my_net_write(net, (char*)packet->ptr(), packet->length()))
  {
    *errmsg = "failed on my_net_write()";
    return -1;
  }
  return 0;
}

static int send_file(THD *thd)
{
  NET* net = &thd->net;
  int fd = -1,bytes, error = 1;
  char fname[FN_REFLEN+1];
  const char *errmsg = 0;
  int old_timeout;
  uint packet_len;
  char buf[IO_SIZE];				// It's safe to alloc this
  DBUG_ENTER("send_file");

  /*
    The client might be slow loading the data, give him wait_timeout to do
    the job
  */
  old_timeout = thd->net.read_timeout;
  thd->net.read_timeout = thd->variables.net_wait_timeout;

  /*
    We need net_flush here because the client will not know it needs to send
    us the file name until it has processed the load event entry
  */
  if (net_flush(net) || (packet_len = my_net_read(net)) == packet_error)
  {
    errmsg = "while reading file name";
    goto err;
  }

  // terminate with \0 for fn_format
  *((char*)net->read_pos +  packet_len) = 0;
  fn_format(fname, (char*) net->read_pos + 1, "", "", 4);
  // this is needed to make replicate-ignore-db
  if (!strcmp(fname,"/dev/null"))
    goto end;

  if ((fd = my_open(fname, O_RDONLY, MYF(0))) < 0)
  {
    errmsg = "on open of file";
    goto err;
  }

  while ((bytes = (int) my_read(fd, (byte*) buf, IO_SIZE, MYF(0))) > 0)
  {
    if (my_net_write(net, buf, bytes))
    {
      errmsg = "while writing data to client";
      goto err;
    }
  }

 end:
  if (my_net_write(net, "", 0) || net_flush(net) ||
      (my_net_read(net) == packet_error))
  {
    errmsg = "while negotiating file transfer close";
    goto err;
  }
  error = 0;

 err:
  thd->net.read_timeout = old_timeout;
  if (fd >= 0)
    (void) my_close(fd, MYF(0));
  if (errmsg)
  {
    sql_print_error("Failed in send_file() %s", errmsg);
    DBUG_PRINT("error", (errmsg));
  }
  DBUG_RETURN(error);
}


File open_binlog(IO_CACHE *log, const char *log_file_name,
		 const char **errmsg)
{
  File file;
  DBUG_ENTER("open_binlog");

  if ((file = my_open(log_file_name, O_RDONLY | O_BINARY, MYF(MY_WME))) < 0 ||
      init_io_cache(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
		    MYF(MY_WME | MY_DONT_CHECK_FILESIZE)))
  {
    *errmsg = "Could not open log file";	// This will not be sent
    goto err;
  }
  if (check_binlog_magic(log,errmsg))
    goto err;
  DBUG_RETURN(file);

err:
  if (file >= 0)
  {
    my_close(file,MYF(0));
    end_io_cache(log);
  }
  DBUG_RETURN(-1);
}


/*
  Adjust the position pointer in the binary log file for all running slaves

  SYNOPSIS
    adjust_linfo_offsets()
    purge_offset	Number of bytes removed from start of log index file

  NOTES
    - This is called when doing a PURGE when we delete lines from the
      index log file

  REQUIREMENTS
    - Before calling this function, we have to ensure that no threads are
      using any binary log file before purge_offset.a

  TODO
    - Inform the slave threads that they should sync the position
      in the binary log file with flush_relay_log_info.
      Now they sync is done for next read.
*/

void adjust_linfo_offsets(my_off_t purge_offset)
{
  THD *tmp;

  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);

  while ((tmp=it++))
  {
    LOG_INFO* linfo;
    if ((linfo = tmp->current_linfo))
    {
      pthread_mutex_lock(&linfo->lock);
      /*
	Index file offset can be less that purge offset only if
	we just started reading the index file. In that case
	we have nothing to adjust
      */
      if (linfo->index_file_offset < purge_offset)
	linfo->fatal = (linfo->index_file_offset != 0);
      else
	linfo->index_file_offset -= purge_offset;
      pthread_mutex_unlock(&linfo->lock);
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
}


bool log_in_use(const char* log_name)
{
  int log_name_len = strlen(log_name) + 1;
  THD *tmp;
  bool result = 0;

  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);

  while ((tmp=it++))
  {
    LOG_INFO* linfo;
    if ((linfo = tmp->current_linfo))
    {
      pthread_mutex_lock(&linfo->lock);
      result = !memcmp(log_name, linfo->log_file_name, log_name_len);
      pthread_mutex_unlock(&linfo->lock);
      if (result)
	break;
    }
  }

  pthread_mutex_unlock(&LOCK_thread_count);
  return result;
}

int purge_error_message(THD* thd, int res)
{
  const char* errmsg = 0;

  switch(res)  {
  case 0: break;
  case LOG_INFO_EOF:	 errmsg = "Target log not found in binlog index"; break;
  case LOG_INFO_IO:	 errmsg = "I/O error reading log index file"; break;
  case LOG_INFO_INVALID: errmsg = "Server configuration does not permit \
binlog purge"; break;
  case LOG_INFO_SEEK:	errmsg = "Failed on fseek()"; break;
  case LOG_INFO_PURGE_NO_ROTATE: errmsg = "Cannot purge unrotatable log";
    break;
  case LOG_INFO_MEM:	errmsg = "Out of memory"; break;
  case LOG_INFO_FATAL:	errmsg = "Fatal error during purge"; break;
  case LOG_INFO_IN_USE: errmsg = "A purgeable log is in use, will not purge";
    break;
  default:		errmsg = "Unknown error during purge"; break;
  }

  if (errmsg)
  {
    send_error(thd, 0, errmsg);
    return 1;
  }
  else
    send_ok(thd);
  return 0;
}

int purge_master_logs(THD* thd, const char* to_log)
{
  char search_file_name[FN_REFLEN];

  mysql_bin_log.make_log_name(search_file_name, to_log);
  int res = mysql_bin_log.purge_logs(thd, search_file_name);

  return purge_error_message(thd, res);
}


int purge_master_logs_before_date(THD* thd, time_t purge_time)
{
  int res = mysql_bin_log.purge_logs_before_date(thd, purge_time);
  return purge_error_message(thd ,res);
}

/*
  TODO: Clean up loop to only have one call to send_file()
*/

void mysql_binlog_send(THD* thd, char* log_ident, my_off_t pos,
		       ushort flags)
{
  LOG_INFO linfo;
  char *log_file_name = linfo.log_file_name;
  char search_file_name[FN_REFLEN], *name;
  IO_CACHE log;
  File file = -1;
  String* packet = &thd->packet;
  int error;
  const char *errmsg = "Unknown error";
  NET* net = &thd->net;
#ifndef DBUG_OFF
  int left_events = max_binlog_dump_events;
#endif
  DBUG_ENTER("mysql_binlog_send");
  DBUG_PRINT("enter",("log_ident: '%s'  pos: %ld", log_ident, (long) pos));

  bzero((char*) &log,sizeof(log));

#ifndef DBUG_OFF
  if (opt_sporadic_binlog_dump_fail && (binlog_dump_count++ % 2))
  {
    errmsg = "Master failed COM_BINLOG_DUMP to test if slave can recover";
    my_errno= ER_UNKNOWN_ERROR;
    goto err;
  }
#endif

  if (!mysql_bin_log.is_open())
  {
    errmsg = "Binary log is not open";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }
  if (!server_id_supplied)
  {
    errmsg = "Misconfigured master - server id was not set";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }

  name=search_file_name;
  if (log_ident[0])
    mysql_bin_log.make_log_name(search_file_name, log_ident);
  else
    name=0;					// Find first log

  linfo.index_file_offset = 0;
  thd->current_linfo = &linfo;

  if (mysql_bin_log.find_log_pos(&linfo, name, 1))
  {
    errmsg = "Could not find first log file name in binary log index file";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }

  if ((file=open_binlog(&log, log_file_name, &errmsg)) < 0)
  {
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }
  if (pos < BIN_LOG_HEADER_SIZE || pos > my_b_filelength(&log))
  {
    errmsg= "Client requested master to start replication from \
impossible position";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    goto err;
  }

  my_b_seek(&log, pos);				// Seek will done on next read
  /*
    We need to start a packet with something other than 255
    to distiquish it from error
  */
  packet->set("\0", 1, &my_charset_bin);

  // if we are at the start of the log
  if (pos == BIN_LOG_HEADER_SIZE)
  {
    // tell the client log name with a fake rotate_event
    if (fake_rotate_event(net, packet, log_file_name, &errmsg))
    {
      my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
      goto err;
    }
    packet->set("\0", 1, &my_charset_bin);
  }

  while (!net->error && net->vio != 0 && !thd->killed)
  {
    pthread_mutex_t *log_lock = mysql_bin_log.get_log_lock();

    while (!(error = Log_event::read_log_event(&log, packet, log_lock)))
    {
#ifndef DBUG_OFF
      if (max_binlog_dump_events && !left_events--)
      {
	net_flush(net);
	errmsg = "Debugging binlog dump abort";
	my_errno= ER_UNKNOWN_ERROR;
	goto err;
      }
#endif
      if (my_net_write(net, (char*)packet->ptr(), packet->length()) )
      {
	errmsg = "Failed on my_net_write()";
	my_errno= ER_UNKNOWN_ERROR;
	goto err;
      }
      DBUG_PRINT("info", ("log event code %d",
			  (*packet)[LOG_EVENT_OFFSET+1] ));
      if ((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
      {
	if (send_file(thd))
	{
	  errmsg = "failed in send_file()";
	  my_errno= ER_UNKNOWN_ERROR;
	  goto err;
	}
      }
      packet->set("\0", 1, &my_charset_bin);
    }
    /*
      TODO: now that we are logging the offset, check to make sure
      the recorded offset and the actual match
    */
    if (error != LOG_READ_EOF)
    {
      my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
      switch (error) {
      case LOG_READ_BOGUS:
	errmsg = "bogus data in log event";
	break;
      case LOG_READ_TOO_LARGE:
	errmsg = "log event entry exceeded max_allowed_packet; \
Increase max_allowed_packet on master";
	break;
      case LOG_READ_IO:
	errmsg = "I/O error reading log event";
	break;
      case LOG_READ_MEM:
	errmsg = "memory allocation failed reading log event";
	break;
      case LOG_READ_TRUNC:
	errmsg = "binlog truncated in the middle of event";
	break;
      default:
	errmsg = "unknown error reading log event on the master";
	break;
      }
      goto err;
    }

    if (!(flags & BINLOG_DUMP_NON_BLOCK) &&
       mysql_bin_log.is_active(log_file_name))
    {
      /*
	Block until there is more data in the log
      */
      if (net_flush(net))
      {
	errmsg = "failed on net_flush()";
	my_errno= ER_UNKNOWN_ERROR;
	goto err;
      }

      /*
	We may have missed the update broadcast from the log
	that has just happened, let's try to catch it if it did.
	If we did not miss anything, we just wait for other threads
	to signal us.
      */
      {
	log.error=0;
	bool read_packet = 0, fatal_error = 0;

#ifndef DBUG_OFF
	if (max_binlog_dump_events && !left_events--)
	{
	  errmsg = "Debugging binlog dump abort";
	  my_errno= ER_UNKNOWN_ERROR;
	  goto err;
	}
#endif

	/*
	  No one will update the log while we are reading
	  now, but we'll be quick and just read one record

	  TODO:
	  Add an counter that is incremented for each time we update
	  the binary log.  We can avoid the following read if the counter
	  has not been updated since last read.
	*/

	pthread_mutex_lock(log_lock);
	switch (Log_event::read_log_event(&log, packet, (pthread_mutex_t*)0)) {
	case 0:
	  /* we read successfully, so we'll need to send it to the slave */
	  pthread_mutex_unlock(log_lock);
	  read_packet = 1;
	  break;

	case LOG_READ_EOF:
	  DBUG_PRINT("wait",("waiting for data in binary log"));
	  if (!thd->killed)
	  {
	    /* Note that the following call unlocks lock_log */
	    mysql_bin_log.wait_for_update(thd);
	  }
	  else
	    pthread_mutex_unlock(log_lock);
	  DBUG_PRINT("wait",("binary log received update"));
	  break;

	default:
	  pthread_mutex_unlock(log_lock);
	  fatal_error = 1;
	  break;
	}

	if (read_packet)
	{
	  thd->proc_info = "sending update to slave";
	  if (my_net_write(net, (char*)packet->ptr(), packet->length()) )
	  {
	    errmsg = "Failed on my_net_write()";
	    my_errno= ER_UNKNOWN_ERROR;
	    goto err;
	  }

	  if ((*packet)[LOG_EVENT_OFFSET+1] == LOAD_EVENT)
	  {
	    if (send_file(thd))
	    {
	      errmsg = "failed in send_file()";
	      my_errno= ER_UNKNOWN_ERROR;
	      goto err;
	    }
	  }
	  packet->set("\0", 1, &my_charset_bin);
	  /*
	    No need to net_flush because we will get to flush later when
	    we hit EOF pretty quick
	  */
	}

	if (fatal_error)
	{
	  errmsg = "error reading log entry";
          my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
	  goto err;
	}
	log.error=0;
      }
    }
    else
    {
      bool loop_breaker = 0;
      // need this to break out of the for loop from switch
      thd->proc_info = "switching to next log";
      switch (mysql_bin_log.find_next_log(&linfo, 1)) {
      case LOG_INFO_EOF:
	loop_breaker = (flags & BINLOG_DUMP_NON_BLOCK);
	break;
      case 0:
	break;
      default:
	errmsg = "could not find next log";
	my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
	goto err;
      }

      if (loop_breaker)
	break;

      end_io_cache(&log);
      (void) my_close(file, MYF(MY_WME));

      // fake Rotate_log event just in case it did not make it to the log
      // otherwise the slave make get confused about the offset
      if ((file=open_binlog(&log, log_file_name, &errmsg)) < 0 ||
	  fake_rotate_event(net, packet, log_file_name, &errmsg))
      {
	my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
	goto err;
      }
      packet->length(0);
      packet->append("\0",1);
    }
  }

  end_io_cache(&log);
  (void)my_close(file, MYF(MY_WME));

  send_eof(thd);
  thd->proc_info = "waiting to finalize termination";
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;

 err:
  thd->proc_info = "waiting to finalize termination";
  end_io_cache(&log);
  /*
    Exclude  iteration through thread list
    this is needed for purge_logs() - it will iterate through
    thread list and update thd->current_linfo->index_file_offset
    this mutex will make sure that it never tried to update our linfo
    after we return from this stack frame
  */
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  if (file >= 0)
    (void) my_close(file, MYF(MY_WME));
  send_error(thd, my_errno, errmsg);
  DBUG_VOID_RETURN;
}

int start_slave(THD* thd , MASTER_INFO* mi,  bool net_report)
{
  int slave_errno;
  if (!thd)
    thd = current_thd;
  int thread_mask;
  DBUG_ENTER("start_slave");
  
  if (check_access(thd, SUPER_ACL, any_db))
    DBUG_RETURN(1);
  lock_slave_threads(mi);  // this allows us to cleanly read slave_running
  // Get a mask of _stopped_ threads
  init_thread_mask(&thread_mask,mi,1 /* inverse */);
  /*
    Below we will start all stopped threads.
    But if the user wants to start only one thread, do as if the other thread
    was running (as we don't wan't to touch the other thread), so set the
    bit to 0 for the other thread
  */
  if (thd->lex.slave_thd_opt)
    thread_mask &= thd->lex.slave_thd_opt;
  if (thread_mask) //some threads are stopped, start them
  {
    if (init_master_info(mi,master_info_file,relay_log_info_file, 0))
      slave_errno=ER_MASTER_INFO;
    else if (server_id_supplied && *mi->host)
      slave_errno = start_slave_threads(0 /*no mutex */,
					1 /* wait for start */,
					mi,
					master_info_file,relay_log_info_file,
					thread_mask);
    else
      slave_errno = ER_BAD_SLAVE;
  }
  else
  {
    //no error if all threads are already started, only a warning
    slave_errno= 0;
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_SLAVE_WAS_RUNNING,
                 ER(ER_SLAVE_WAS_RUNNING));
  }
  
  unlock_slave_threads(mi);
  
  if (slave_errno)
  {
    if (net_report)
      send_error(thd, slave_errno);
    DBUG_RETURN(1);
  }
  else if (net_report)
    send_ok(thd);

  DBUG_RETURN(0);
}


int stop_slave(THD* thd, MASTER_INFO* mi, bool net_report )
{
  int slave_errno;
  if (!thd)
    thd = current_thd;

  if (check_access(thd, SUPER_ACL, any_db))
    return 1;
  thd->proc_info = "Killing slave";
  int thread_mask;
  lock_slave_threads(mi);
  // Get a mask of _running_ threads
  init_thread_mask(&thread_mask,mi,0 /* not inverse*/);
  /*
    Below we will stop all running threads.
    But if the user wants to stop only one thread, do as if the other thread
    was stopped (as we don't wan't to touch the other thread), so set the
    bit to 0 for the other thread
  */
  if (thd->lex.slave_thd_opt)
    thread_mask &= thd->lex.slave_thd_opt;

  if (thread_mask)
  {
    slave_errno= terminate_slave_threads(mi,thread_mask,
                                         1 /*skip lock */);
  }
  else
  {
    //no error if both threads are already stopped, only a warning
    slave_errno= 0;
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE, ER_SLAVE_WAS_NOT_RUNNING,
                 ER(ER_SLAVE_WAS_NOT_RUNNING));
  }
  unlock_slave_threads(mi);
  thd->proc_info = 0;

  if (slave_errno)
  {
    if (net_report)
      send_error(thd, slave_errno);
    return 1;
  }
  else if (net_report)
    send_ok(thd);

  return 0;
}


/*
  Remove all relay logs and start replication from the start

  SYNOPSIS
    reset_slave()
    thd			Thread handler
    mi			Master info for the slave


  NOTES
    We don't send ok in this functions as this is called from
    reload_acl_and_cache() which may have done other tasks, which may
    have failed for which we want to send and error.

  RETURN
    0	ok
    1	error
	In this case error is sent to the client with send_error()
*/


int reset_slave(THD *thd, MASTER_INFO* mi)
{
  MY_STAT stat_area;
  char fname[FN_REFLEN];
  int thread_mask= 0, error= 0;
  uint sql_errno=0;
  const char* errmsg=0;
  DBUG_ENTER("reset_slave");

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /* not inverse */);
  if (thread_mask) // We refuse if any slave thread is running
  {
    sql_errno= ER_SLAVE_MUST_STOP;
    error=1;
    goto err;
  }
  //delete relay logs, clear relay log coordinates
  if ((error= purge_relay_logs(&mi->rli, thd,
			       1 /* just reset */,
			       &errmsg)))
    goto err;
  
  //Clear master's log coordinates (only for good display of SHOW SLAVE STATUS)
  mi->master_log_name[0]= 0;
  mi->master_log_pos= BIN_LOG_HEADER_SIZE;
  //close master_info_file, relay_log_info_file, set mi->inited=rli->inited=0
  end_master_info(mi);
  //and delete these two files
  fn_format(fname, master_info_file, mysql_data_home, "", 4+32);
  if (my_stat(fname, &stat_area, MYF(0)) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }
  fn_format(fname, relay_log_info_file, mysql_data_home, "", 4+32);
  if (my_stat(fname, &stat_area, MYF(0)) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }

err:
  unlock_slave_threads(mi);
  if (thd && error) 
    send_error(thd, sql_errno, errmsg);
  DBUG_RETURN(error);
}


void kill_zombie_dump_threads(uint32 slave_server_id)
{
  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);
  THD *tmp;

  while ((tmp=it++))
  {
    if (tmp->command == COM_BINLOG_DUMP &&
       tmp->server_id == slave_server_id)
    {
      pthread_mutex_lock(&tmp->LOCK_delete);	// Lock from delete
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  if (tmp)
  {
    /*
      Here we do not call kill_one_thread() as
      it will be slow because it will iterate through the list
      again. We just to do kill the thread ourselves.
    */
    tmp->awake(1/*prepare to die*/);
    pthread_mutex_unlock(&tmp->LOCK_delete);
  }
}


int change_master(THD* thd, MASTER_INFO* mi)
{
  int thread_mask;
  const char* errmsg=0;
  bool need_relay_log_purge=1;
  DBUG_ENTER("change_master");

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /*not inverse*/);
  if (thread_mask) // We refuse if any slave thread is running
  {
    net_printf(thd,ER_SLAVE_MUST_STOP);
    unlock_slave_threads(mi);
    DBUG_RETURN(1);
  }

  thd->proc_info = "changing master";
  LEX_MASTER_INFO* lex_mi = &thd->lex.mi;
  // TODO: see if needs re-write
  if (init_master_info(mi, master_info_file, relay_log_info_file, 0))
  {
    send_error(thd, 0, "Could not initialize master info");
    unlock_slave_threads(mi);
    DBUG_RETURN(1);
  }

  /*
    Data lock not needed since we have already stopped the running threads,
    and we have the hold on the run locks which will keep all threads that
    could possibly modify the data structures from running
  */
  if ((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name && !lex_mi->pos)
  {
    // if we change host or port, we must reset the postion
    mi->master_log_name[0] = 0;
    mi->master_log_pos= BIN_LOG_HEADER_SIZE;
    mi->rli.pending = 0;
  }

  if (lex_mi->log_file_name)
    strmake(mi->master_log_name, lex_mi->log_file_name,
	    sizeof(mi->master_log_name));
  if (lex_mi->pos)
  {
    mi->master_log_pos= lex_mi->pos;
    mi->rli.pending = 0;
  }
  DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));

  if (lex_mi->host)
    strmake(mi->host, lex_mi->host, sizeof(mi->host));
  if (lex_mi->user)
    strmake(mi->user, lex_mi->user, sizeof(mi->user));
  if (lex_mi->password)
    strmake(mi->password, lex_mi->password, sizeof(mi->password));
  if (lex_mi->port)
    mi->port = lex_mi->port;
  if (lex_mi->connect_retry)
    mi->connect_retry = lex_mi->connect_retry;

  if (lex_mi->relay_log_name)
  {
    need_relay_log_purge= 0;
    strmake(mi->rli.relay_log_name,lex_mi->relay_log_name,
	    sizeof(mi->rli.relay_log_name)-1);
  }

  if (lex_mi->relay_log_pos)
  {
    need_relay_log_purge= 0;
    mi->rli.relay_log_pos=lex_mi->relay_log_pos;
  }

  flush_master_info(mi);
  if (need_relay_log_purge)
  {
    mi->rli.skip_log_purge= 0;
    thd->proc_info="purging old relay logs";
    if (purge_relay_logs(&mi->rli, thd,
			 0 /* not only reset, but also reinit */,
			 &errmsg))
    {
      net_printf(thd, 0, "Failed purging old relay logs: %s",errmsg);
      unlock_slave_threads(mi);
      DBUG_RETURN(1);
    }
  }
  else
  {
    const char* msg;
    mi->rli.skip_log_purge= 1;
    /* Relay log is already initialized */
    if (init_relay_log_pos(&mi->rli,
			   mi->rli.relay_log_name,
			   mi->rli.relay_log_pos,
			   0 /*no data lock*/,
			   &msg))
    {
      net_printf(thd,0,"Failed initializing relay log position: %s",msg);
      unlock_slave_threads(mi);
      DBUG_RETURN(1);
    }
  }
  mi->rli.master_log_pos = mi->master_log_pos;
  DBUG_PRINT("info", ("master_log_pos: %d", (ulong) mi->master_log_pos));
  strmake(mi->rli.master_log_name,mi->master_log_name,
	  sizeof(mi->rli.master_log_name)-1);
  if (!mi->rli.master_log_name[0]) // uninitialized case
    mi->rli.master_log_pos=0;

  pthread_mutex_lock(&mi->rli.data_lock);
  mi->rli.abort_pos_wait++;
  pthread_cond_broadcast(&mi->data_cond);
  pthread_mutex_unlock(&mi->rli.data_lock);

  unlock_slave_threads(mi);
  thd->proc_info = 0;
  send_ok(thd);
  DBUG_RETURN(0);
}

int reset_master(THD* thd)
{
  if (!mysql_bin_log.is_open())
  {
    my_error(ER_FLUSH_MASTER_BINLOG_CLOSED,  MYF(ME_BELL+ME_WAITTANG));
    return 1;
  }
  return mysql_bin_log.reset_logs(thd);
}

int cmp_master_pos(const char* log_file_name1, ulonglong log_pos1,
		   const char* log_file_name2, ulonglong log_pos2)
{
  int res;
  uint log_file_name1_len=  strlen(log_file_name1);
  uint log_file_name2_len=  strlen(log_file_name2);

  //  We assume that both log names match up to '.'
  if (log_file_name1_len == log_file_name2_len)
  {
    if ((res= strcmp(log_file_name1, log_file_name2)))
      return res;
    return (log_pos1 < log_pos2) ? -1 : (log_pos1 == log_pos2) ? 0 : 1;
  }
  return ((log_file_name1_len < log_file_name2_len) ? -1 : 1);
}


int show_binlog_events(THD* thd)
{
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_binlog_events");
  List<Item> field_list;
  const char *errmsg = 0;
  IO_CACHE log;
  File file = -1;

  Log_event::init_show_field_list(&field_list);
  if (protocol-> send_fields(&field_list, 1))
    DBUG_RETURN(-1);

  if (mysql_bin_log.is_open())
  {
    LEX_MASTER_INFO *lex_mi = &thd->lex.mi;
    ha_rows event_count, limit_start, limit_end;
    my_off_t pos = lex_mi->pos;
    char search_file_name[FN_REFLEN], *name;
    const char *log_file_name = lex_mi->log_file_name;
    pthread_mutex_t *log_lock = mysql_bin_log.get_log_lock();
    LOG_INFO linfo;
    Log_event* ev;
  
    limit_start = thd->lex.current_select->offset_limit;
    limit_end = thd->lex.current_select->select_limit + limit_start;

    name= search_file_name;
    if (log_file_name)
      mysql_bin_log.make_log_name(search_file_name, log_file_name);
    else
      name=0;					// Find first log

    linfo.index_file_offset = 0;
    thd->current_linfo = &linfo;

    if (mysql_bin_log.find_log_pos(&linfo, name, 1))
    {
      errmsg = "Could not find target log";
      goto err;
    }

    if ((file=open_binlog(&log, linfo.log_file_name, &errmsg)) < 0)
      goto err;

    if (pos < 4)
    {
      errmsg = "Invalid log position";
      goto err;
    }

    pthread_mutex_lock(log_lock);
    my_b_seek(&log, pos);

    for (event_count = 0;
	 (ev = Log_event::read_log_event(&log,(pthread_mutex_t*)0,0)); )
    {
      if (event_count >= limit_start &&
	  ev->net_send(protocol, linfo.log_file_name, pos))
      {
	errmsg = "Net error";
	delete ev;
	pthread_mutex_unlock(log_lock);
	goto err;
      }

      pos = my_b_tell(&log);
      delete ev;

      if (++event_count >= limit_end)
	break;
    }

    if (event_count < limit_end && log.error)
    {
      errmsg = "Wrong offset or I/O error";
      pthread_mutex_unlock(log_lock);
      goto err;
    }

    pthread_mutex_unlock(log_lock);
  }

err:
  if (file >= 0)
  {
    end_io_cache(&log);
    (void) my_close(file, MYF(MY_WME));
  }

  if (errmsg)
  {
    my_error(ER_ERROR_WHEN_EXECUTING_COMMAND, MYF(0),
	     "SHOW BINLOG EVENTS", errmsg);
    DBUG_RETURN(-1);
  }

  send_eof(thd);
  DBUG_RETURN(0);
}


int show_binlog_info(THD* thd)
{
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_binlog_info");
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("File", FN_REFLEN));
  field_list.push_back(new Item_return_int("Position",20,
					   MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Binlog_do_db",255));
  field_list.push_back(new Item_empty_string("Binlog_ignore_db",255));

  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(-1);
  protocol->prepare_for_resend();

  if (mysql_bin_log.is_open())
  {
    LOG_INFO li;
    mysql_bin_log.get_current_log(&li);
    int dir_len = dirname_length(li.log_file_name);
    protocol->store(li.log_file_name + dir_len, &my_charset_bin);
    protocol->store((ulonglong) li.pos);
    protocol->store(&binlog_do_db);
    protocol->store(&binlog_ignore_db);
    if (protocol->write())
      DBUG_RETURN(-1);
  }
  send_eof(thd);
  DBUG_RETURN(0);
}


/*
  Send a lost of all binary logs to client

  SYNOPSIS
    show_binlogs()
    thd		Thread specific variable

  RETURN VALUES
    0	ok
    1	error  (Error message sent to client)
*/

int show_binlogs(THD* thd)
{
  const char *errmsg;
  IO_CACHE *index_file;
  char fname[FN_REFLEN];
  NET* net = &thd->net;
  List<Item> field_list;
  String *packet = &thd->packet;
  uint length;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_binlogs");

  if (!mysql_bin_log.is_open())
  {
    //TODO:  Replace with ER() error message
    errmsg= "You are not using binary logging";
    goto err_with_msg;
  }

  field_list.push_back(new Item_empty_string("Log_name", 255));
  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(1);
  mysql_bin_log.lock_index();
  index_file=mysql_bin_log.get_index_file();
  
  reinit_io_cache(index_file, READ_CACHE, (my_off_t) 0, 0, 0);

  /* The file ends with EOF or empty line */
  while ((length=my_b_gets(index_file, fname, sizeof(fname))) > 1)
  {
    protocol->prepare_for_resend();
    int dir_len = dirname_length(fname);
    /* The -1 is for removing newline from fname */
    protocol->store(fname + dir_len, length-1-dir_len, &my_charset_bin);
    if (protocol->write())
      goto err;
  }
  mysql_bin_log.unlock_index();
  send_eof(thd);
  DBUG_RETURN(0);

err_with_msg:
  send_error(thd, ER_UNKNOWN_ERROR, errmsg);
err:
  mysql_bin_log.unlock_index();
  DBUG_RETURN(1);
}


int log_loaded_block(IO_CACHE* file)
{
  LOAD_FILE_INFO* lf_info;
  uint block_len ;

  /* file->request_pos contains position where we started last read */
  char* buffer = (char*) file->request_pos;
  if (!(block_len = (char*) file->read_end - (char*) buffer))
    return 0;
  lf_info = (LOAD_FILE_INFO*) file->arg;
  if (lf_info->last_pos_in_file != HA_POS_ERROR &&
      lf_info->last_pos_in_file >= file->pos_in_file)
    return 0;
  lf_info->last_pos_in_file = file->pos_in_file;
  if (lf_info->wrote_create_file)
  {
    Append_block_log_event a(lf_info->thd, buffer, block_len,
			     lf_info->log_delayed);
    mysql_bin_log.write(&a);
  }
  else
  {
    Create_file_log_event c(lf_info->thd,lf_info->ex,lf_info->db,
			    lf_info->table_name, *lf_info->fields,
			    lf_info->handle_dup, buffer,
			    block_len, lf_info->log_delayed);
    mysql_bin_log.write(&c);
    lf_info->wrote_create_file = 1;
    DBUG_SYNC_POINT("debug_lock.created_file_event",10);
  }
  return 0;
}

#endif /* HAVE_REPLICATION */


