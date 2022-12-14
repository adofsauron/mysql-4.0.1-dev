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
#include "repl_failsafe.h"
#include "sql_repl.h"
#include "slave.h"
#include "sql_acl.h"
#include "mini_client.h"
#include "log_event.h"
#include <mysql.h>
#include <thr_alarm.h>

#define SLAVE_LIST_CHUNK 128
#define SLAVE_ERRMSG_SIZE (FN_REFLEN+64)


RPL_STATUS rpl_status=RPL_NULL;
pthread_mutex_t LOCK_rpl_status;
pthread_cond_t COND_rpl_status;
HASH slave_list;
extern const char* any_db;

const char *rpl_role_type[] = {"MASTER","SLAVE",NullS};
TYPELIB rpl_role_typelib = {array_elements(rpl_role_type)-1,"",
			    rpl_role_type};

const char* rpl_status_type[] = {"AUTH_MASTER","ACTIVE_SLAVE","IDLE_SLAVE",
				 "LOST_SOLDIER","TROOP_SOLDIER",
				 "RECOVERY_CAPTAIN","NULL",NullS};
TYPELIB rpl_status_typelib= {array_elements(rpl_status_type)-1,"",
			     rpl_status_type};

static Slave_log_event* find_slave_event(IO_CACHE* log,
					 const char* log_file_name,
					 char* errmsg);

static int init_failsafe_rpl_thread(THD* thd)
{
  DBUG_ENTER("init_failsafe_rpl_thread");
  thd->system_thread = thd->bootstrap = 1;
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->net.timeout = slave_net_timeout;
  thd->max_packet_length=thd->net.max_packet;
  thd->master_access= ~0;
  thd->priv_user = 0;
  thd->system_thread = 1;
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id = thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  if (init_thr_lock() ||
      my_pthread_setspecific_ptr(THR_THD,  thd) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root) ||
      my_pthread_setspecific_ptr(THR_NET,  &thd->net))
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES); // is this needed?
    end_thread(thd,0);
    DBUG_RETURN(-1);
  }

  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
#if !defined(__WIN__) && !defined(OS2)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  thd->mem_root.free=thd->mem_root.used=0;	
  if (thd->max_join_size == (ulong) ~0L)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info="Thread initialized";
  thd->version=refresh_version;
  thd->set_time();
  DBUG_RETURN(0);
}

void change_rpl_status(RPL_STATUS from_status, RPL_STATUS to_status)
{
  pthread_mutex_lock(&LOCK_rpl_status);
  if (rpl_status == from_status || rpl_status == RPL_ANY)
    rpl_status = to_status;
  pthread_cond_signal(&COND_rpl_status);
  pthread_mutex_unlock(&LOCK_rpl_status);
}

#define get_object(p, obj) \
{\
  uint len = (uint)*p++;  \
  if (p + len > p_end || len >= sizeof(obj)) \
    goto err; \
  strmake(obj,(char*) p,len); \
  p+= len; \
}\

static inline int cmp_master_pos(Slave_log_event* sev, LEX_MASTER_INFO* mi)
{
  return cmp_master_pos(sev->master_log, sev->master_pos, mi->log_file_name,
			mi->pos);
}

void unregister_slave(THD* thd, bool only_mine, bool need_mutex)
{
  if (need_mutex)
    pthread_mutex_lock(&LOCK_slave_list);
  if (thd->server_id)
  {
    SLAVE_INFO* old_si;
    if ((old_si = (SLAVE_INFO*)hash_search(&slave_list,
					   (byte*)&thd->server_id, 4)) &&
	(!only_mine || old_si->thd == thd))
    hash_delete(&slave_list, (byte*)old_si);
  }
  if (need_mutex)
    pthread_mutex_unlock(&LOCK_slave_list);
}

int register_slave(THD* thd, uchar* packet, uint packet_length)
{
  SLAVE_INFO *si;
  int res = 1;
  uchar* p = packet, *p_end = packet + packet_length;

  if (check_access(thd, FILE_ACL, any_db))
    return 1;

  if (!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
    goto err;

  thd->server_id = si->server_id = uint4korr(p);
  p += 4;
  get_object(p,si->host);
  get_object(p,si->user);
  get_object(p,si->password);
  si->port = uint2korr(p);
  p += 2;
  si->rpl_recovery_rank = uint4korr(p);
  p += 4;
  if (!(si->master_id = uint4korr(p)))
    si->master_id = server_id;
  si->thd = thd;
  pthread_mutex_lock(&LOCK_slave_list);

  unregister_slave(thd,0,0);
  res = hash_insert(&slave_list, (byte*) si);
  pthread_mutex_unlock(&LOCK_slave_list);
  return res;

err:
  if (si)
    my_free((gptr) si, MYF(MY_WME));
  return res;
}

static uint32* slave_list_key(SLAVE_INFO* si, uint* len,
			     my_bool not_used __attribute__((unused)))
{
  *len = 4;
  return &si->server_id;
}

static void slave_info_free(void *s)
{
  my_free((gptr) s, MYF(MY_WME));
}

void init_slave_list()
{
  hash_init(&slave_list, SLAVE_LIST_CHUNK, 0, 0,
	    (hash_get_key) slave_list_key, slave_info_free, 0);
  pthread_mutex_init(&LOCK_slave_list, MY_MUTEX_INIT_FAST);
}

void end_slave_list()
{
  /* No protection by a mutex needed as we are only called at shutdown */
  if (hash_inited(&slave_list))
  {
    hash_free(&slave_list);
    pthread_mutex_destroy(&LOCK_slave_list);
  }
}

static int find_target_pos(LEX_MASTER_INFO* mi, IO_CACHE* log, char* errmsg)
{
  uint32 log_seq = mi->last_log_seq;
  uint32 target_server_id = mi->server_id;

  for (;;)
  {
    Log_event* ev;
    if (!(ev = Log_event::read_log_event(log, (pthread_mutex_t*)0,
					 0)))
    {
      if (log->error > 0)
	strmov(errmsg, "Binary log truncated in the middle of event");
      else if (log->error < 0)
	strmov(errmsg, "I/O error reading binary log");
      else
	strmov(errmsg, "Could not find target event in the binary log");
      return 1;
    }

    if (ev->log_seq == log_seq && ev->server_id == target_server_id)
    {
      delete ev;
      mi->pos = my_b_tell(log);
      return 0;
    }

    delete ev;
  }
}


int translate_master(THD* thd, LEX_MASTER_INFO* mi, char* errmsg)
{
  LOG_INFO linfo;
  char search_file_name[FN_REFLEN],last_log_name[FN_REFLEN];
  IO_CACHE log;
  File file = -1, last_file = -1;
  pthread_mutex_t *log_lock;
  const char* errmsg_p;
  Slave_log_event* sev = 0;
  my_off_t last_pos = 0;
  int error = 1;
  int cmp_res;
  LINT_INIT(cmp_res);

  if (!mysql_bin_log.is_open())
  {
    strmov(errmsg,"Binary log is not open");
    return 1;
  }

  if (!server_id_supplied)
  {
    strmov(errmsg, "Misconfigured master - server id was not set");
    return 1;
  }

  linfo.index_file_offset = 0;


  search_file_name[0] = 0;

  if (mysql_bin_log.find_first_log(&linfo, search_file_name))
  {
    strmov(errmsg,"Could not find first log");
    return 1;
  }
  thd->current_linfo = &linfo;

  bzero((char*) &log,sizeof(log));
  log_lock = mysql_bin_log.get_log_lock();
  pthread_mutex_lock(log_lock);

  for (;;)
  {
    if ((file=open_binlog(&log, linfo.log_file_name, &errmsg_p)) < 0)
    {
      strmov(errmsg, errmsg_p);
      goto err;
    }

    if (!(sev = find_slave_event(&log, linfo.log_file_name, errmsg)))
      goto err;

    cmp_res = cmp_master_pos(sev, mi);
    delete sev;

    if (!cmp_res)
    {
      /* Copy basename */
      fn_format(mi->log_file_name, linfo.log_file_name, "","",1);
      mi->pos = my_b_tell(&log);
      goto mi_inited;
    }
    else if (cmp_res > 0)
    {
      if (!last_pos)
      {
	strmov(errmsg,
	       "Slave event in first log points past the target position");
	goto err;
      }
      end_io_cache(&log);
      (void) my_close(file, MYF(MY_WME));
      if (init_io_cache(&log, (file = last_file), IO_SIZE, READ_CACHE, 0, 0,
			MYF(MY_WME)))
      {
	errmsg[0] = 0;
	goto err;
      }
      break;
    }

    strmov(last_log_name, linfo.log_file_name);
    last_pos = my_b_tell(&log);

    switch (mysql_bin_log.find_next_log(&linfo)) {
    case LOG_INFO_EOF:
      if (last_file >= 0)
       (void)my_close(last_file, MYF(MY_WME));
      last_file = -1;
      goto found_log;
    case 0:
      break;
    default:
      strmov(errmsg, "Error reading log index");
      goto err;
    }

    end_io_cache(&log);
    if (last_file >= 0)
     (void) my_close(last_file, MYF(MY_WME));
    last_file = file;
  }

found_log:
  my_b_seek(&log, last_pos);
  if (find_target_pos(mi,&log,errmsg))
    goto err;
  fn_format(mi->log_file_name, last_log_name, "","",1);  /* Copy basename */

mi_inited:
  error = 0;
err:
  pthread_mutex_unlock(log_lock);
  end_io_cache(&log);
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  if (file >= 0)
    (void) my_close(file, MYF(MY_WME));
  if (last_file >= 0 && last_file != file)
    (void) my_close(last_file, MYF(MY_WME));

  return error;
}

// caller must delete result when done
static Slave_log_event* find_slave_event(IO_CACHE* log,
					 const char* log_file_name,
					 char* errmsg)
{
  Log_event* ev;
  int i;
  bool slave_event_found = 0;
  LINT_INIT(ev);

  for (i = 0; i < 2; i++)
  {
    if (!(ev = Log_event::read_log_event(log, (pthread_mutex_t*)0, 0)))
    {
      my_snprintf(errmsg, SLAVE_ERRMSG_SIZE,
		  "Error reading event in log '%s'",
		  (char*)log_file_name);
      return 0;
    }
    if (ev->get_type_code() == SLAVE_EVENT)
    {
      slave_event_found = 1;
      break;
    }
    delete ev;
  }
  if (!slave_event_found)
  {
    my_snprintf(errmsg, SLAVE_ERRMSG_SIZE,
		"Could not find slave event in log '%s'",
		(char*)log_file_name);
    delete ev;
    return 0;
  }

  return (Slave_log_event*)ev;
}


int show_new_master(THD* thd)
{
  DBUG_ENTER("show_new_master");
  List<Item> field_list;
  char errmsg[SLAVE_ERRMSG_SIZE];
  LEX_MASTER_INFO* lex_mi = &thd->lex.mi;

  errmsg[0]=0;					// Safety
  if (translate_master(thd, lex_mi, errmsg))
  {
    if (errmsg[0])
      net_printf(&thd->net, ER_ERROR_WHEN_EXECUTING_COMMAND,
		 "SHOW NEW MASTER", errmsg);
    else
      send_error(&thd->net, 0);

    DBUG_RETURN(1);
  }
  else
  {
    String* packet = &thd->packet;
    field_list.push_back(new Item_empty_string("Log_name", 20));
    field_list.push_back(new Item_empty_string("Log_pos", 20));
    if (send_fields(thd, field_list, 1))
      DBUG_RETURN(-1);
    packet->length(0);
    net_store_data(packet, lex_mi->log_file_name);
    net_store_data(packet, (longlong)lex_mi->pos);
    if (my_net_write(&thd->net, packet->ptr(), packet->length()))
      DBUG_RETURN(-1);
    send_eof(&thd->net);
    DBUG_RETURN(0);
  }
}

int update_slave_list(MYSQL* mysql)
{
  MYSQL_RES* res=0;
  MYSQL_ROW row;
  const char* error=0;
  bool have_auth_info;
  int port_ind;

  if (mc_mysql_query(mysql,"SHOW SLAVE HOSTS",0) ||
      !(res = mc_mysql_store_result(mysql)))
  {
    error = "Query error";
    goto err;
  }

  switch (mc_mysql_num_fields(res))
  {
  case 5:
    have_auth_info = 0;
    port_ind=2;
    break;
  case 7:
    have_auth_info = 1;
    port_ind=4;
    break;
  default:
    error = "Invalid number of fields in SHOW SLAVE HOSTS";
    goto err;
  }

  pthread_mutex_lock(&LOCK_slave_list);

  while ((row = mc_mysql_fetch_row(res)))
  {
    uint32 server_id;
    SLAVE_INFO* si, *old_si;
    server_id = atoi(row[0]);
    if ((old_si = (SLAVE_INFO*)hash_search(&slave_list,
					   (byte*)&server_id,4)))
      si = old_si;
    else
    {
      if (!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
      {
	error = "Out of memory";
	pthread_mutex_unlock(&LOCK_slave_list);
	goto err;
      }
      si->server_id = server_id;
      hash_insert(&slave_list, (byte*)si);
    }
    strnmov(si->host, row[1], sizeof(si->host));
    si->port = atoi(row[port_ind]);
    si->rpl_recovery_rank = atoi(row[port_ind+1]);
    si->master_id = atoi(row[port_ind+2]);
    if (have_auth_info)
    {
      strnmov(si->user, row[2], sizeof(si->user));
      strnmov(si->password, row[3], sizeof(si->password));
    }
  }
  pthread_mutex_unlock(&LOCK_slave_list);
err:
  if (res)
    mc_mysql_free_result(res);
  if (error)
  {
    sql_print_error("Error updating slave list:",error);
    return 1;
  }
  return 0;
}

int find_recovery_captain(THD* thd, MYSQL* mysql)
{

  return 0;
}

pthread_handler_decl(handle_failsafe_rpl,arg)
{
  DBUG_ENTER("handle_failsafe_rpl");
  THD *thd = new THD;
  thd->thread_stack = (char*)&thd;
  MYSQL* recovery_captain = 0;
  pthread_detach_this_thread();
  if (init_failsafe_rpl_thread(thd) || !(recovery_captain=mc_mysql_init(0)))
  {
    sql_print_error("Could not initialize failsafe replication thread");
    goto err;
  }
  pthread_mutex_lock(&LOCK_rpl_status);
  while (!thd->killed && !abort_loop)
  {
    bool break_req_chain = 0;
    const char* msg = thd->enter_cond(&COND_rpl_status,
				      &LOCK_rpl_status, "Waiting for request");
    pthread_cond_wait(&COND_rpl_status, &LOCK_rpl_status);
    thd->proc_info="Processling request";
    while (!break_req_chain)
    {
      switch (rpl_status)
      {
      case RPL_LOST_SOLDIER:
	if (find_recovery_captain(thd, recovery_captain))
	  rpl_status=RPL_TROOP_SOLDIER;
	else
	  rpl_status=RPL_RECOVERY_CAPTAIN;
	break_req_chain=1; /* for now until other states are implemented */
	break;
      default:
	break_req_chain=1;
	break;
      }
    }
    thd->exit_cond(msg);
  }
  pthread_mutex_unlock(&LOCK_rpl_status);
err:
  if (recovery_captain)
    mc_mysql_close(recovery_captain);
  delete thd;
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);
}

int show_slave_hosts(THD* thd)
{
  List<Item> field_list;
  NET* net = &thd->net;
  String* packet = &thd->packet;
  DBUG_ENTER("show_slave_hosts");

  field_list.push_back(new Item_empty_string("Server_id", 20));
  field_list.push_back(new Item_empty_string("Host", 20));
  if (opt_show_slave_auth_info)
  {
    field_list.push_back(new Item_empty_string("User",20));
    field_list.push_back(new Item_empty_string("Password",20));
  }
  field_list.push_back(new Item_empty_string("Port",20));
  field_list.push_back(new Item_empty_string("Rpl_recovery_rank", 20));
  field_list.push_back(new Item_empty_string("Master_id", 20));

  if (send_fields(thd, field_list, 1))
    DBUG_RETURN(-1);

  pthread_mutex_lock(&LOCK_slave_list);

  for (uint i = 0; i < slave_list.records; ++i)
  {
    SLAVE_INFO* si = (SLAVE_INFO*) hash_element(&slave_list, i);
    packet->length(0);
    net_store_data(packet, si->server_id);
    net_store_data(packet, si->host);
    if (opt_show_slave_auth_info)
    {
      net_store_data(packet, si->user);
      net_store_data(packet, si->password);
    }
    net_store_data(packet, (uint32) si->port);
    net_store_data(packet, si->rpl_recovery_rank);
    net_store_data(packet, si->master_id);
    if (my_net_write(net, (char*)packet->ptr(), packet->length()))
    {
      pthread_mutex_unlock(&LOCK_slave_list);
      DBUG_RETURN(-1);
    }
  }
  pthread_mutex_unlock(&LOCK_slave_list);
  send_eof(net);
  DBUG_RETURN(0);
}

int connect_to_master(THD *thd, MYSQL* mysql, MASTER_INFO* mi)
{
  if (!mc_mysql_connect(mysql, mi->host, mi->user, mi->password, 0,
		   mi->port, 0, 0))
  {
    sql_print_error("Connection to master failed: %s",
		    mc_mysql_error(mysql));
    return 1;
  }
  return 0;
}


static inline void cleanup_mysql_results(MYSQL_RES* db_res,
				  MYSQL_RES** cur, MYSQL_RES** start)
{
  for( ; cur >= start; --cur)
  {
    if (*cur)
      mc_mysql_free_result(*cur);
  }
  mc_mysql_free_result(db_res);
}


static inline int fetch_db_tables(THD* thd, MYSQL* mysql, const char* db,
				  MYSQL_RES* table_res)
{
  MYSQL_ROW row;

  for( row = mc_mysql_fetch_row(table_res); row;
       row = mc_mysql_fetch_row(table_res))
  {
    TABLE_LIST table;
    const char* table_name = row[0];
    int error;
    if (table_rules_on)
    {
      table.next = 0;
      table.db = (char*)db;
      table.real_name = (char*)table_name;
      table.updating = 1;
      if (!tables_ok(thd, &table))
	continue;
    }

    if ((error = fetch_nx_table(thd, db, table_name, &glob_mi, mysql)))
      return error;
  }

  return 0;
}


int load_master_data(THD* thd)
{
  MYSQL mysql;
  MYSQL_RES* master_status_res = 0;
  bool slave_was_running = 0;
  int error = 0;

  mc_mysql_init(&mysql);

  // we do not want anyone messing with the slave at all for the entire
  // duration of the data load;
  pthread_mutex_lock(&LOCK_slave);

  // first, kill the slave
  if ((slave_was_running = slave_running))
  {
    abort_slave = 1;
    KICK_SLAVE;
    thd->proc_info = "waiting for slave to die";
    while (slave_running)
      pthread_cond_wait(&COND_slave_stopped, &LOCK_slave); // wait until done
  }


  if (connect_to_master(thd, &mysql, &glob_mi))
  {
    net_printf(&thd->net, error = ER_CONNECT_TO_MASTER,
		 mc_mysql_error(&mysql));
    goto err;
  }

  // now that we are connected, get all database and tables in each
  {
    MYSQL_RES *db_res, **table_res, **table_res_end, **cur_table_res;
    uint num_dbs;

    if (mc_mysql_query(&mysql, "show databases", 0) ||
	!(db_res = mc_mysql_store_result(&mysql)))
    {
      net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		 mc_mysql_error(&mysql));
      goto err;
    }

    if (!(num_dbs = (uint) mc_mysql_num_rows(db_res)))
      goto err;
    // in theory, the master could have no databases at all
    // and run with skip-grant

    if (!(table_res = (MYSQL_RES**)thd->alloc(num_dbs * sizeof(MYSQL_RES*))))
    {
      net_printf(&thd->net, error = ER_OUTOFMEMORY);
      goto err;
    }

    // this is a temporary solution until we have online backup
    // capabilities - to be replaced once online backup is working
    // we wait to issue FLUSH TABLES WITH READ LOCK for as long as we
    // can to minimize the lock time
    if (mc_mysql_query(&mysql, "FLUSH TABLES WITH READ LOCK", 0) ||
	mc_mysql_query(&mysql, "SHOW MASTER STATUS",0) ||
	!(master_status_res = mc_mysql_store_result(&mysql)))
    {
      net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		 mc_mysql_error(&mysql));
      goto err;
    }

    // go through every table in every database, and if the replication
    // rules allow replicating it, get it

    table_res_end = table_res + num_dbs;

    for(cur_table_res = table_res; cur_table_res < table_res_end;
	cur_table_res++)
    {
      // since we know how many rows we have, this can never be NULL
      MYSQL_ROW row = mc_mysql_fetch_row(db_res);
      char* db = row[0];

      /*
	Do not replicate databases excluded by rules
	also skip mysql database - in most cases the user will
	mess up and not exclude mysql database with the rules when
	he actually means to - in this case, he is up for a surprise if
	his priv tables get dropped and downloaded from master
	TO DO - add special option, not enabled
	by default, to allow inclusion of mysql database into load
	data from master
      */

      if (!db_ok(db, replicate_do_db, replicate_ignore_db) ||
	 !strcmp(db,"mysql"))
      {
	*cur_table_res = 0;
	continue;
      }

      if (mysql_rm_db(thd, db, 1,1) ||
	  mysql_create_db(thd, db, 0, 1))
      {
	send_error(&thd->net, 0, 0);
	cleanup_mysql_results(db_res, cur_table_res - 1, table_res);
	goto err;
      }

      if (mc_mysql_select_db(&mysql, db) ||
	  mc_mysql_query(&mysql, "show tables", 0) ||
	 !(*cur_table_res = mc_mysql_store_result(&mysql)))
      {
	net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		   mc_mysql_error(&mysql));
	cleanup_mysql_results(db_res, cur_table_res - 1, table_res);
	goto err;
      }

      if ((error = fetch_db_tables(thd, &mysql, db, *cur_table_res)))
      {
	// we do not report the error - fetch_db_tables handles it
	cleanup_mysql_results(db_res, cur_table_res, table_res);
	goto err;
      }
    }

    cleanup_mysql_results(db_res, cur_table_res - 1, table_res);

    // adjust position in the master
    if (master_status_res)
    {
      MYSQL_ROW row = mc_mysql_fetch_row(master_status_res);

      /*
	We need this check because the master may not be running with
	log-bin, but it will still allow us to do all the steps
	of LOAD DATA FROM MASTER - no reason to forbid it, really,
	 although it does not make much sense for the user to do it
      */
      if (row[0] && row[1])
      {
	strmake(glob_mi.log_file_name, row[0], sizeof(glob_mi.log_file_name));
	glob_mi.pos = atoi(row[1]); // atoi() is ok, since offset is <= 1GB
	if (glob_mi.pos < 4)
	  glob_mi.pos = 4;			// don't hit the magic number
	glob_mi.pending = 0;
	flush_master_info(&glob_mi);
      }

      mc_mysql_free_result(master_status_res);
    }

    if (mc_mysql_query(&mysql, "UNLOCK TABLES", 0))
    {
      net_printf(&thd->net, error = ER_QUERY_ON_MASTER,
		 mc_mysql_error(&mysql));
      goto err;
    }
  }

err:
  pthread_mutex_unlock(&LOCK_slave);
  if (slave_was_running)
    start_slave(0, 0);
  mc_mysql_close(&mysql); // safe to call since we always do mc_mysql_init()
  if (!error)
    send_ok(&thd->net);

  return error;
}
