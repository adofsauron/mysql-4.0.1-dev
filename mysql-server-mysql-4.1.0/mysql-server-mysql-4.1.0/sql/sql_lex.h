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


/* YACC and LEX Definitions */

/* These may not be declared yet */
class Table_ident;
class sql_exchange;
class LEX_COLUMN;

/*
  The following hack is needed because mysql_yacc.cc does not define
  YYSTYPE before including this file
*/

#include "set_var.h"

#ifdef MYSQL_YACC
#define LEX_YYSTYPE void *
#else
#include "lex_symbol.h"
#include "sql_yacc.h"
#define LEX_YYSTYPE YYSTYPE *
#endif

/*
  When a command is added here, be sure it's also added in mysqld.cc
  in "struct show_var_st status_vars[]= {" ...
*/

enum enum_sql_command {
  SQLCOM_SELECT, SQLCOM_CREATE_TABLE, SQLCOM_CREATE_INDEX, SQLCOM_ALTER_TABLE,
  SQLCOM_UPDATE, SQLCOM_INSERT, SQLCOM_INSERT_SELECT,
  SQLCOM_DELETE, SQLCOM_TRUNCATE, SQLCOM_DROP_TABLE, SQLCOM_DROP_INDEX,

  SQLCOM_SHOW_DATABASES, SQLCOM_SHOW_TABLES, SQLCOM_SHOW_FIELDS,
  SQLCOM_SHOW_KEYS, SQLCOM_SHOW_VARIABLES, SQLCOM_SHOW_LOGS, SQLCOM_SHOW_STATUS,
  SQLCOM_SHOW_INNODB_STATUS,
  SQLCOM_SHOW_PROCESSLIST, SQLCOM_SHOW_MASTER_STAT, SQLCOM_SHOW_SLAVE_STAT,
  SQLCOM_SHOW_GRANTS, SQLCOM_SHOW_CREATE, SQLCOM_SHOW_CHARSETS,
  SQLCOM_SHOW_COLLATIONS, SQLCOM_SHOW_CREATE_DB,

  SQLCOM_LOAD,SQLCOM_SET_OPTION,SQLCOM_LOCK_TABLES,SQLCOM_UNLOCK_TABLES,
  SQLCOM_GRANT,
  SQLCOM_CHANGE_DB, SQLCOM_CREATE_DB, SQLCOM_DROP_DB, SQLCOM_ALTER_DB,
  SQLCOM_REPAIR, SQLCOM_REPLACE, SQLCOM_REPLACE_SELECT,
  SQLCOM_CREATE_FUNCTION, SQLCOM_DROP_FUNCTION,
  SQLCOM_REVOKE,SQLCOM_OPTIMIZE, SQLCOM_CHECK,
  SQLCOM_FLUSH, SQLCOM_KILL,  SQLCOM_ANALYZE,
  SQLCOM_ROLLBACK, SQLCOM_COMMIT, SQLCOM_SLAVE_START, SQLCOM_SLAVE_STOP,
  SQLCOM_BEGIN, SQLCOM_LOAD_MASTER_TABLE, SQLCOM_CHANGE_MASTER,
  SQLCOM_RENAME_TABLE, SQLCOM_BACKUP_TABLE, SQLCOM_RESTORE_TABLE,
  SQLCOM_RESET, SQLCOM_PURGE, SQLCOM_PURGE_BEFORE, SQLCOM_SHOW_BINLOGS,
  SQLCOM_SHOW_OPEN_TABLES, SQLCOM_LOAD_MASTER_DATA,
  SQLCOM_HA_OPEN, SQLCOM_HA_CLOSE, SQLCOM_HA_READ,
  SQLCOM_SHOW_SLAVE_HOSTS, SQLCOM_DELETE_MULTI, SQLCOM_UPDATE_MULTI,
  SQLCOM_SHOW_BINLOG_EVENTS, SQLCOM_SHOW_NEW_MASTER, SQLCOM_DO,
  SQLCOM_SHOW_WARNS, SQLCOM_EMPTY_QUERY, SQLCOM_SHOW_ERRORS,
  SQLCOM_SHOW_COLUMN_TYPES, SQLCOM_SHOW_TABLE_TYPES, SQLCOM_SHOW_PRIVILEGES,
  SQLCOM_HELP,

  /* This should be the last !!! */
  SQLCOM_END
};


typedef List<Item> List_item;

typedef struct st_lex_master_info
{
  char *host, *user, *password, *log_file_name;
  uint port, connect_retry;
  ulonglong pos;
  ulong server_id;
  char *relay_log_name;
  ulong relay_log_pos;
} LEX_MASTER_INFO;


enum sub_select_type
{
  UNSPECIFIED_TYPE,UNION_TYPE, INTERSECT_TYPE,
  EXCEPT_TYPE, GLOBAL_OPTIONS_TYPE, DERIVED_TABLE_TYPE, OLAP_TYPE
};

enum olap_type 
{
  UNSPECIFIED_OLAP_TYPE, CUBE_TYPE, ROLLUP_TYPE
};

/* 
  The state of the lex parsing for selects 
   
   All select describing structures linked with following pointers:
   - list of neighbors (next/prev) (prev of first element point to slave 
     pointer of upper structure)
     - one level units for unit (union) structure
     - member of one union(unit) for ordinary select_lex
   - pointer to master
     - outer select_lex for unit (union)
     - unit structure for ordinary select_lex
   - pointer to slave
     - first list element of select_lex belonged to this unit for unit
     - first unit in list of units that belong to this select_lex (as
       subselects or derived tables) for ordinary select_lex
   - list of all select_lex (for group operation like correcting list of opened
     tables)
   for example for following query:

   select *
     from table1
     where table1.field IN (select * from table1_1_1 union
                            select * from table1_1_2)
     union
   select *
     from table2
     where table2.field=(select (select f1 from table2_1_1_1_1
                                   where table2_1_1_1_1.f2=table2_1_1.f3)
                           from table2_1_1
                           where table2_1_1.f1=table2.f2)
     union
   select * from table3;

   we will have following structure:


     main unit
     select1 select2 select3
     |^^     |^
    s|||     ||master
    l|||     |+---------------------------------+
    a|||     +---------------------------------+|
    v|||master                         slave   ||
    e||+-------------------------+             ||
     V|            neighbor      |             V|
     unit 1.1<==================>unit1.2       unit2.1
     select1.1.1 select 1.1.2    select1.2.1   select2.1.1 select2.1.2
                                               |^
                                               ||
                                               V|
                                               unit2.1.1.1
                                               select2.1.1.1.1


   relation in main unit will be following:
                          
         main unit
         |^^^
         ||||
         |||+------------------------------+
         ||+--------------+                |
    slave||master         |                |
         V|      neighbor |       neighbor |
         select1<========>select2<========>select3

    list of all select_lex will be following (as it will be constructed by
    parser):

    select1->select2->select3->select2.1.1->select 2.1.2->select2.1.1.1.1-+
                                                                          |
    +---------------------------------------------------------------------+
    |
    +->select1.1.1->select1.1.2

*/

/* 
    Base class for st_select_lex (SELECT_LEX) & 
    st_select_lex_unit (SELECT_LEX_UNIT)
*/
struct st_lex;
class st_select_lex;
class st_select_lex_unit;
class st_select_lex_node {
protected:
  st_select_lex_node *next, **prev,   /* neighbor list */
    *master, *slave,                  /* vertical links */
    *link_next, **link_prev;          /* list of whole SELECT_LEX */
public:
  ulong options;
  enum sub_select_type linkage;
  SQL_LIST order_list;                /* ORDER clause */
  List<List_item>     expr_list;
  List<List_item>     when_list;      /* WHEN clause (expression) */
  ha_rows select_limit, offset_limit; /* LIMIT clause parameters */
  // Arrays of pointers to top elements of all_fields list
  Item **ref_pointer_array;

  uint with_sum_func;   /* sum function indicator and number of it */
  bool create_refs;
  bool dependent;	/* dependent from outer select subselect */
  bool uncacheable;     /* result of this query can't be cached */
  bool no_table_names_allowed; /* used for global order by */

  static void *operator new(size_t size)
  {
    // TODO: Change to alloc() and explicitely clear elements in constructors
    return (void*) sql_calloc((uint) size);
  }
  static void operator delete(void *ptr,size_t size) {}
  st_select_lex_node(): linkage(UNSPECIFIED_TYPE) {}
  virtual ~st_select_lex_node() {}
  inline st_select_lex_node* get_master() { return master; }
  virtual void init_query();
  virtual void init_select();
  void include_down(st_select_lex_node *upper);
  void include_neighbour(st_select_lex_node *before);
  void include_global(st_select_lex_node **plink);
  void exclude();

  virtual st_select_lex* select_lex();
  virtual bool add_item_to_list(THD *thd, Item *item);
  bool add_order_to_list(THD *thd, Item *item, bool asc);
  virtual bool add_group_to_list(THD *thd, Item *item, bool asc);
  virtual bool add_ftfunc_to_list(Item_func_match *func);

  virtual st_select_lex_unit* master_unit()= 0;
  virtual st_select_lex* outer_select()= 0;

  virtual bool set_braces(bool value);
  virtual bool inc_in_sum_expr();
  virtual uint get_in_sum_expr();
  virtual TABLE_LIST* get_table_list();
  virtual List<Item>* get_item_list();
  virtual List<String>* get_use_index();
  virtual List<String>* get_ignore_index();
  virtual ulong get_table_join_options();
  virtual TABLE_LIST *add_table_to_list(THD *thd, Table_ident *table,
					LEX_STRING *alias,
					ulong table_options,
					thr_lock_type flags= TL_UNLOCK,
					List<String> *use_index= 0,
					List<String> *ignore_index= 0);
  virtual void set_lock_for_tables(thr_lock_type lock_type) {}
  void mark_as_dependent(st_select_lex *last);

  friend class st_select_lex_unit;
  friend bool mysql_new_select(struct st_lex *lex, bool move_down);
private:
  void fast_exclude();
};
typedef class st_select_lex_node SELECT_LEX_NODE;

/* 
   SELECT_LEX_UNIT - unit of selects (UNION, INTERSECT, ...) group 
   SELECT_LEXs
*/
struct st_lex;
class THD;
class select_result;
class JOIN;
class select_union;
class st_select_lex_unit: public st_select_lex_node {
protected:
  List<Item> item_list; 
  TABLE_LIST result_table_list;
  select_union *union_result;
  TABLE *table; /* temporary table using for appending UNION results */

  select_result *result;
  int res;
  bool describe, found_rows_for_union,
    prepared, // prepare phase already performed for UNION (unit)
    optimized, // optimize phase already performed for UNION (unit)
    executed, // already executed
    t_and_f;  // used for transferring tables_and_fields_initied UNIT:: methods 
public:
  /*
    Pointer to 'last' select or pointer to unit where stored
    global parameters for union
  */
  st_select_lex_node *global_parameters;
  /* LIMIT clause runtime counters */
  ha_rows select_limit_cnt, offset_limit_cnt;
  /* not NULL if union used in subselect, point to subselect item */
  Item_subselect *item;
  THD *thd;

  uint union_option;

  void init_query();
  bool create_total_list(THD *thd, st_lex *lex, TABLE_LIST **result,
			 bool check_current_derived);
  st_select_lex_unit* master_unit();
  st_select_lex* outer_select();
  st_select_lex* first_select() { return (st_select_lex*) slave; }
  st_select_lex* first_select_in_union() 
  { 
    return (slave && slave->linkage == GLOBAL_OPTIONS_TYPE) ?
      (st_select_lex*) slave->next : (st_select_lex*) slave;
  }
  st_select_lex_unit* next_unit() { return (st_select_lex_unit*) next; }
  void exclude_level();

  /* UNION methods */
  int prepare(THD *thd, select_result *result, bool tables_and_fields_initied);
  int exec();
  int cleanup();
  
  friend void mysql_init_query(THD *thd);
  friend int subselect_union_engine::exec();
private:
  bool create_total_list_n_last_return(THD *thd, st_lex *lex,
				       TABLE_LIST ***result,
				       bool check_current_derived);
};
typedef class st_select_lex_unit SELECT_LEX_UNIT;

/*
  SELECT_LEX - store information of parsed SELECT_LEX statment
*/
class st_select_lex: public st_select_lex_node
{
public:
  char *db, *db1, *table1, *db2, *table2;      	/* For outer join using .. */
  Item *where, *having;                         /* WHERE & HAVING clauses */
  enum olap_type olap;
  SQL_LIST	      table_list, group_list;   /* FROM & GROUP BY clauses */
  List<Item>          item_list; /* list of fields & expressions */
  List<String>        interval_list, use_index, *use_index_ptr,
		      ignore_index, *ignore_index_ptr;
  /* 
    Usualy it is pointer to ftfunc_list_alloc, but in union used to create fake
    select_lex for calling mysql_select under results of union
  */
  List<Item_func_match> *ftfunc_list;
  List<Item_func_match> ftfunc_list_alloc;
  JOIN *join; /* after JOIN::prepare it is pointer to corresponding JOIN */
  const char *type; /* type of select for EXPLAIN */
  ulong table_join_options;
  uint in_sum_expr;
  uint select_number; /* number of select (used for EXPLAIN) */
  uint with_wild; /* item list contain '*' */
  bool  braces;   	/* SELECT ... UNION (SELECT ... ) <- this braces */
  /* TRUE when having fix field called in processing of this SELECT */
  bool having_fix_field;

  void init_query();
  void init_select();
  st_select_lex_unit* master_unit();
  st_select_lex_unit* first_inner_unit() 
  { 
    return (st_select_lex_unit*) slave; 
  }
  st_select_lex* outer_select();
  st_select_lex* next_select() { return (st_select_lex*) next; }
  st_select_lex* next_select_in_list() 
  {
    return (st_select_lex*) link_next;
  }
  st_select_lex_node** next_select_in_list_addr()
  {
    return &link_next;
  }

  bool set_braces(bool value);
  bool inc_in_sum_expr();
  uint get_in_sum_expr();

  st_select_lex* select_lex();
  bool add_item_to_list(THD *thd, Item *item);
  bool add_group_to_list(THD *thd, Item *item, bool asc);
  bool add_ftfunc_to_list(Item_func_match *func);

  TABLE_LIST* get_table_list();
  List<Item>* get_item_list();
  List<String>* get_use_index();
  List<String>* get_ignore_index();
  ulong get_table_join_options();
  TABLE_LIST* add_table_to_list(THD *thd, Table_ident *table,
				LEX_STRING *alias,
				ulong table_options,
				thr_lock_type flags= TL_UNLOCK,
				List<String> *use_index= 0,
				List<String> *ignore_index= 0);
  void set_lock_for_tables(thr_lock_type lock_type);
  inline void init_order()
  {
    order_list.elements= 0;
    order_list.first= 0;
    order_list.next= (byte**) &order_list.first;
  }
  
  friend void mysql_init_query(THD *thd);
  void make_empty_select(st_select_lex *last_select)
  {
    select_number=INT_MAX;
    init_query();
    init_select();
    include_neighbour(last_select);
  }
};
typedef class st_select_lex SELECT_LEX;


/* The state of the lex parsing. This is saved in the THD struct */

typedef struct st_lex
{
  uint	 yylineno,yytoklen;			/* Simulate lex */
  LEX_YYSTYPE yylval;
  SELECT_LEX_UNIT unit;                         /* most upper unit */
  SELECT_LEX select_lex;                        /* first SELECT_LEX */
  /* current SELECT_LEX in parsing */
  SELECT_LEX_NODE *current_select;
  /* list of all SELECT_LEX */
  SELECT_LEX *all_selects_list;
  uchar *ptr,*tok_start,*tok_end,*end_of_query;
  char *length,*dec,*change,*name;
  char *backup_dir;				/* For RESTORE/BACKUP */
  char* to_log;                                 /* For PURGE MASTER LOGS TO */
  time_t purge_time;                            /* For PURGE MASTER LOGS BEFORE */
  char* x509_subject,*x509_issuer,*ssl_cipher;
  char* found_colon;                            /* For multi queries - next query */
  enum SSL_type ssl_type;			/* defined in violite.h */
  String *wild;
  sql_exchange *exchange;
  select_result *result;

  List<key_part_spec> col_list;
  List<key_part_spec> ref_list;
  List<Alter_drop>    drop_list;
  List<Alter_column>  alter_list;
  List<String>	      interval_list;
  List<LEX_USER>      users_list;
  List<LEX_COLUMN>    columns;
  List<Key>	      key_list;
  List<create_field>  create_list;
  List<Item>	      *insert_list,field_list,value_list;
  List<List_item>     many_values;
  List<set_var_base>  var_list;
  List<Item>          param_list;
  SQL_LIST	      proc_list, auxilliary_table_list;
  TYPELIB	      *interval;
  create_field	      *last_field;
  Item *default_value, *comment;
  CHARSET_INFO *thd_charset;
  uint uint_geom_type;
  LEX_USER *grant_user;
  gptr yacc_yyss,yacc_yyvs;
  THD *thd;
  udf_func udf;
  HA_CHECK_OPT   check_opt;			// check/repair options
  HA_CREATE_INFO create_info;
  LEX_MASTER_INFO mi;				// used by CHANGE MASTER
  USER_RESOURCES mqh;
  ulong thread_id,type;
  enum_sql_command sql_command;
  thr_lock_type lock_option;
  enum my_lex_states next_state;
  enum enum_duplicates duplicates;
  enum enum_tx_isolation tx_isolation;
  enum enum_ha_read_modes ha_read_mode;
  enum ha_rkey_function ha_rkey_mode;
  enum enum_enable_or_disable alter_keys_onoff;
  enum enum_var_type option_type;
  uint grant, grant_tot_col, which_columns;
  uint fk_delete_opt, fk_update_opt, fk_match_option;
  uint param_count;
  bool drop_primary, drop_if_exists, drop_temporary, local_file;
  bool in_comment, ignore_space, verbose, simple_alter;
  bool derived_tables, describe;
  bool safe_to_cache_query;
  uint slave_thd_opt;
  CHARSET_INFO *charset;
  char *help_arg;
  SQL_LIST *gorder_list;

  inline void uncacheable()
  {
    safe_to_cache_query= 0;

    /*
      There are no sense to mark select_lex and union fields of LEX,
      but we should merk all subselects as uncacheable from current till
      most upper
    */
    SELECT_LEX_NODE *sl;
    SELECT_LEX_UNIT *un;
    for (sl= current_select, un= sl->master_unit();
	 un != &unit;
	 sl= sl->outer_select(), un= sl->master_unit())
    {
      sl->uncacheable = un->uncacheable= 1;
    }
  }
} LEX;


void lex_init(void);
void lex_free(void);
LEX *lex_start(THD *thd, uchar *buf,uint length);
void lex_end(LEX *lex);

extern pthread_key(LEX*,THR_LEX);

extern LEX_STRING tmp_table_alias;

#define current_lex (&current_thd->lex)
