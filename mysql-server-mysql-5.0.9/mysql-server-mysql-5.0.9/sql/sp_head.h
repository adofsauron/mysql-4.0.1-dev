/* -*- C++ -*- */
/* Copyright (C) 2002 MySQL AB

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

#ifndef _SP_HEAD_H_
#define _SP_HEAD_H_

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include <stddef.h>

// Values for the type enum. This reflects the order of the enum declaration
// in the CREATE TABLE command.
#define TYPE_ENUM_FUNCTION  1
#define TYPE_ENUM_PROCEDURE 2
#define TYPE_ENUM_TRIGGER   3

Item_result
sp_map_result_type(enum enum_field_types type);

bool
sp_multi_results_command(enum enum_sql_command cmd);

struct sp_label;
class sp_instr;
struct sp_cond_type;
struct sp_pvar;

class sp_name : public Sql_alloc
{
public:

  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_qname;

  sp_name(LEX_STRING name)
    : m_name(name)
  {
    m_db.str= m_qname.str= 0;
    m_db.length= m_qname.length= 0;
  }

  sp_name(LEX_STRING db, LEX_STRING name)
    : m_db(db), m_name(name)
  {
    m_qname.str= 0;
    m_qname.length= 0;
  }

  // Init. the qualified name from the db and name.
  void init_qname(THD *thd);	// thd for memroot allocation

  ~sp_name()
  {}
};

sp_name *
sp_name_current_db_new(THD *thd, LEX_STRING name);


class sp_head :private Query_arena
{
  sp_head(const sp_head &);	/* Prevent use of these */
  void operator=(sp_head &);

  MEM_ROOT main_mem_root;
public:

  int m_type;			// TYPE_ENUM_FUNCTION or TYPE_ENUM_PROCEDURE
  enum enum_field_types m_returns; // For FUNCTIONs only
  CHARSET_INFO *m_returns_cs;	// For FUNCTIONs only
  TYPELIB *m_returns_typelib;	// For FUNCTIONs only
  uint m_returns_len;		// For FUNCTIONs only
  uint m_returns_pack;		// For FUNCTIONs only
  my_bool m_has_return;		// For FUNCTIONs only
  my_bool m_simple_case;	// TRUE if parsing simple case, FALSE otherwise
  my_bool m_multi_results;	// TRUE if a procedure with SELECT(s)
  my_bool m_in_handler;		// TRUE if parser in a handler body
  uchar *m_tmp_query;		// Temporary pointer to sub query string
  uint m_old_cmq;		// Old CLIENT_MULTI_QUERIES value
  st_sp_chistics *m_chistics;
  ulong m_sql_mode;		// For SHOW CREATE
  LEX_STRING m_qname;		// db.name
  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_params;
  LEX_STRING m_body;
  LEX_STRING m_defstr;
  LEX_STRING m_definer_user;
  LEX_STRING m_definer_host;
  longlong m_created;
  longlong m_modified;
  /*
    Sets containing names of SP and SF used by this routine.

    TODO Probably we should combine these two hashes in one. It will
    decrease memory overhead ans simplify algorithms using them. The
    same applies to similar hashes in LEX.
  */
  HASH m_spfuns, m_spprocs;
  // Pointers set during parsing
  uchar *m_param_begin, *m_param_end, *m_body_begin;

  static void *
  operator new(size_t size);

  static void
  operator delete(void *ptr, size_t size);

  sp_head();

  // Initialize after we have reset mem_root
  void
  init(LEX *lex);

  // Initialize strings after parsing header
  void
  init_strings(THD *thd, LEX *lex, sp_name *name);

  TYPELIB *
  create_typelib(List<String> *src);

  int
  create(THD *thd);

  virtual ~sp_head();

  // Free memory
  void
  destroy();

  int
  execute_function(THD *thd, Item **args, uint argcount, Item **resp);

  int
  execute_procedure(THD *thd, List<Item> *args);

  int
  show_create_procedure(THD *thd);

  int
  show_create_function(THD *thd);

  void
  add_instr(sp_instr *instr);

  inline uint
  instructions()
  {
    return m_instr.elements;
  }

  inline sp_instr *
  last_instruction()
  {
    sp_instr *i;

    get_dynamic(&m_instr, (gptr)&i, m_instr.elements-1);
    return i;
  }

  // Resets lex in 'thd' and keeps a copy of the old one.
  void
  reset_lex(THD *thd);

  // Restores lex in 'thd' from our copy, but keeps some status from the
  // one in 'thd', like ptr, tables, fields, etc.
  void
  restore_lex(THD *thd);

  // Put the instruction on the backpatch list, associated with the label.
  void
  push_backpatch(sp_instr *, struct sp_label *);

  // Update all instruction with this label in the backpatch list to
  // the current position.
  void
  backpatch(struct sp_label *);

  // Check that no unresolved references exist.
  // If none found, 0 is returned, otherwise errors have been issued
  // and -1 is returned.
  // This is called by the parser at the end of a create procedure/function.
  int
  check_backpatch(THD *thd);

  char *name(uint *lenp = 0) const
  {
    if (lenp)
      *lenp= m_name.length;
    return m_name.str;
  }

  char *create_string(THD *thd, ulong *lenp);

  Field *make_field(uint max_length, const char *name, TABLE *dummy);

  void set_info(char *definer, uint definerlen,
		longlong created, longlong modified,
		st_sp_chistics *chistics, ulong sql_mode);

  void reset_thd_mem_root(THD *thd);

  void restore_thd_mem_root(THD *thd);

  void optimize();
  void opt_mark(uint ip);

  inline sp_instr *
  get_instr(uint i)
  {
    sp_instr *ip;

    if (i < m_instr.elements)
      get_dynamic(&m_instr, (gptr)&ip, i);
    else
      ip= NULL;
    return ip;
  }

  /* Add tables used by routine to the table list. */
  bool add_used_tables_to_table_list(THD *thd,
                                     TABLE_LIST ***query_tables_last_ptr);

private:

  MEM_ROOT *m_thd_root;		// Temp. store for thd's mem_root
  THD *m_thd;			// Set if we have reset mem_root
  char *m_thd_db;		// Original thd->db pointer

  sp_pcontext *m_pcont;		// Parse context
  List<LEX> m_lex;		// Temp. store for the other lex
  DYNAMIC_ARRAY m_instr;	// The "instructions"
  typedef struct
  {
    struct sp_label *lab;
    sp_instr *instr;
  } bp_t;
  List<bp_t> m_backpatch;	// Instructions needing backpatching
  /*
    Multi-set representing optimized list of tables to be locked by this
    routine. Does not include tables which are used by invoked routines.
  */
  HASH m_sptabs;

  /* Used for tracking of routine invocations and preventing recursion. */
  bool m_is_invoked;

  int
  execute(THD *thd);

  /*
    Merge the list of tables used by query into the multi-set of tables used
    by routine.
  */
  bool merge_table_list(THD *thd, TABLE_LIST *table, LEX *lex_for_tmp_check);
}; // class sp_head : public Sql_alloc


//
// "Instructions"...
//

class sp_instr :public Query_arena, public Sql_alloc
{
  sp_instr(const sp_instr &);	/* Prevent use of these */
  void operator=(sp_instr &);

public:

  uint marked;
  uint m_ip;			// My index
  sp_pcontext *m_ctx;		// My parse context

  // Should give each a name or type code for debugging purposes?
  sp_instr(uint ip, sp_pcontext *ctx)
    :Query_arena(0, INITIALIZED_FOR_SP), marked(0), m_ip(ip), m_ctx(ctx)
  {}

  virtual ~sp_instr()
  { free_items(); }

  // Execute this instrution. '*nextp' will be set to the index of the next
  // instruction to execute. (For most instruction this will be the
  // instruction following this one.)
  // Returns 0 on success, non-zero if some error occured.
  virtual int execute(THD *thd, uint *nextp) = 0;

  /*
    Execute core function of instruction after all preparations (e.g.
    setting of proper LEX, saving part of the thread context have been
    done).

    Should be implemented for instructions using expressions or whole
    statements (thus having to have own LEX). Used in concert with
    sp_lex_keeper class and its descendants.
  */
  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str) = 0;

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx)
  {}

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return m_ip+1;
  }

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  virtual void opt_move(uint dst, List<sp_instr> *ibp)
  {
    m_ip= dst;
  }

}; // class sp_instr : public Sql_alloc


/*
  Auxilary class to which instructions delegate responsibility
  for handling LEX and preparations before executing statement
  or calculating complex expression.

  Exist mainly to avoid having double hierarchy between instruction
  classes.

  TODO: Add ability to not store LEX and do any preparations if
        expression used is simple.
*/

class sp_lex_keeper
{
  /* Prevent use of these */
  sp_lex_keeper(const sp_lex_keeper &);
  void operator=(sp_lex_keeper &);
public:

  sp_lex_keeper(LEX *lex, bool lex_resp)
    : m_lex(lex), m_lex_resp(lex_resp)
  {
    lex->sp_lex_in_use= TRUE;
  }
  virtual ~sp_lex_keeper()
  {
    if (m_lex_resp)
      delete m_lex;
  }

  /*
    Prepare execution of instruction using LEX, if requested check whenever
    we have read access to tables used and open/lock them, call instruction's
    exec_core() method, perform cleanup afterwards.
  */
  int reset_lex_and_exec_core(THD *thd, uint *nextp, bool open_tables,
                              sp_instr* instr);

  inline uint sql_command() const
  {
    return (uint)m_lex->sql_command;
  }

  void disable_query_cache()
  {
    m_lex->safe_to_cache_query= 0;
  }
private:

  LEX *m_lex;
  /*
    Indicates whenever this sp_lex_keeper instance responsible
    for LEX deletion.
  */
  bool m_lex_resp;
};


//
// Call out to some prepared SQL statement.
//
class sp_instr_stmt : public sp_instr
{
  sp_instr_stmt(const sp_instr_stmt &);	/* Prevent use of these */
  void operator=(sp_instr_stmt &);

public:

  LEX_STRING m_query;		// For thd->query

  sp_instr_stmt(uint ip, sp_pcontext *ctx, LEX *lex)
    : sp_instr(ip, ctx), m_lex_keeper(lex, TRUE)
  {
    m_query.str= 0;
    m_query.length= 0;
  }

  virtual ~sp_instr_stmt()
  {};

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_stmt : public sp_instr


class sp_instr_set : public sp_instr
{
  sp_instr_set(const sp_instr_set &);	/* Prevent use of these */
  void operator=(sp_instr_set &);

public:

  sp_instr_set(uint ip, sp_pcontext *ctx,
	       uint offset, Item *val, enum enum_field_types type,
               LEX *lex, bool lex_resp)
    : sp_instr(ip, ctx), m_offset(offset), m_value(val), m_type(type),
      m_lex_keeper(lex, lex_resp)
  {}

  virtual ~sp_instr_set()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_offset;		// Frame offset
  Item *m_value;
  enum enum_field_types m_type;	// The declared type
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_set : public sp_instr


/*
  Set NEW/OLD row field value instruction. Used in triggers.
*/
class sp_instr_set_trigger_field : public sp_instr
{
  sp_instr_set_trigger_field(const sp_instr_set_trigger_field &);
  void operator=(sp_instr_set_trigger_field &);

public:

  sp_instr_set_trigger_field(uint ip, sp_pcontext *ctx,
                             Item_trigger_field *trg_fld, Item *val)
    : sp_instr(ip, ctx),
      trigger_field(trg_fld),
      value(val)
  {}

  virtual ~sp_instr_set_trigger_field()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:
  Item_trigger_field *trigger_field;
  Item *value;
}; // class sp_instr_trigger_field : public sp_instr


class sp_instr_jump : public sp_instr
{
  sp_instr_jump(const sp_instr_jump &);	/* Prevent use of these */
  void operator=(sp_instr_jump &);

public:

  uint m_dest;			// Where we will go

  sp_instr_jump(uint ip, sp_pcontext *ctx)
    : sp_instr(ip, ctx), m_dest(0), m_optdest(0)
  {}

  sp_instr_jump(uint ip, sp_pcontext *ctx, uint dest)
    : sp_instr(ip, ctx), m_dest(dest), m_optdest(0)
  {}

  virtual ~sp_instr_jump()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start);

  virtual void opt_move(uint dst, List<sp_instr> *ibp);

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx)
  {
    if (m_dest == 0)		// Don't reset
      m_dest= dest;
  }

protected:

  sp_instr *m_optdest;		// Used during optimization

}; // class sp_instr_jump : public sp_instr


class sp_instr_jump_if : public sp_instr_jump
{
  sp_instr_jump_if(const sp_instr_jump_if &); /* Prevent use of these */
  void operator=(sp_instr_jump_if &);

public:

  sp_instr_jump_if(uint ip, sp_pcontext *ctx, Item *i, LEX *lex)
    : sp_instr_jump(ip, ctx), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  sp_instr_jump_if(uint ip, sp_pcontext *ctx, Item *i, uint dest, LEX *lex)
    : sp_instr_jump(ip, ctx, dest), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_jump_if()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

private:

  Item *m_expr;			// The condition
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_jump_if : public sp_instr_jump


class sp_instr_jump_if_not : public sp_instr_jump
{
  sp_instr_jump_if_not(const sp_instr_jump_if_not &); /* Prevent use of these */
  void operator=(sp_instr_jump_if_not &);

public:

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i, LEX *lex)
    : sp_instr_jump(ip, ctx), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i, uint dest, LEX *lex)
    : sp_instr_jump(ip, ctx, dest), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_jump_if_not()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

private:

  Item *m_expr;			// The condition
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_jump_if_not : public sp_instr_jump


class sp_instr_freturn : public sp_instr
{
  sp_instr_freturn(const sp_instr_freturn &);	/* Prevent use of these */
  void operator=(sp_instr_freturn &);

public:

  sp_instr_freturn(uint ip, sp_pcontext *ctx,
		   Item *val, enum enum_field_types type, LEX *lex)
    : sp_instr(ip, ctx), m_value(val), m_type(type), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_freturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return UINT_MAX;
  }

protected:

  Item *m_value;
  enum enum_field_types m_type;
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_freturn : public sp_instr


class sp_instr_hpush_jump : public sp_instr_jump
{
  sp_instr_hpush_jump(const sp_instr_hpush_jump &); /* Prevent use of these */
  void operator=(sp_instr_hpush_jump &);

public:

  sp_instr_hpush_jump(uint ip, sp_pcontext *ctx, int htype, uint fp)
    : sp_instr_jump(ip, ctx), m_type(htype), m_frame(fp)
  {
    m_handler= ip+1;
    m_cond.empty();
  }

  virtual ~sp_instr_hpush_jump()
  {
    m_cond.empty();
  }

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  inline void add_condition(struct sp_cond_type *cond)
  {
    m_cond.push_front(cond);
  }

private:

  int m_type;			// Handler type
  uint m_frame;
  uint m_handler;		// Location of handler
  List<struct sp_cond_type> m_cond;

}; // class sp_instr_hpush_jump : public sp_instr_jump


class sp_instr_hpop : public sp_instr
{
  sp_instr_hpop(const sp_instr_hpop &);	/* Prevent use of these */
  void operator=(sp_instr_hpop &);

public:

  sp_instr_hpop(uint ip, sp_pcontext *ctx, uint count)
    : sp_instr(ip, ctx), m_count(count)
  {}

  virtual ~sp_instr_hpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx);

  virtual uint opt_mark(sp_head *sp)
  {
    if (m_count)
      marked= 1;
    return m_ip+1;
  }

private:

  uint m_count;

}; // class sp_instr_hpop : public sp_instr


class sp_instr_hreturn : public sp_instr_jump
{
  sp_instr_hreturn(const sp_instr_hreturn &);	/* Prevent use of these */
  void operator=(sp_instr_hreturn &);

public:

  sp_instr_hreturn(uint ip, sp_pcontext *ctx, uint fp)
    : sp_instr_jump(ip, ctx), m_frame(fp)
  {}

  virtual ~sp_instr_hreturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

private:

  uint m_frame;

}; // class sp_instr_hreturn : public sp_instr


class sp_instr_cpush : public sp_instr
{
  sp_instr_cpush(const sp_instr_cpush &); /* Prevent use of these */
  void operator=(sp_instr_cpush &);

public:

  sp_instr_cpush(uint ip, sp_pcontext *ctx, LEX *lex)
    : sp_instr(ip, ctx), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_cpush()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_cpush : public sp_instr


class sp_instr_cpop : public sp_instr
{
  sp_instr_cpop(const sp_instr_cpop &); /* Prevent use of these */
  void operator=(sp_instr_cpop &);

public:

  sp_instr_cpop(uint ip, sp_pcontext *ctx, uint count)
    : sp_instr(ip, ctx), m_count(count)
  {}

  virtual ~sp_instr_cpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx);

  virtual uint opt_mark(sp_head *sp)
  {
    if (m_count)
      marked= 1;
    return m_ip+1;
  }

private:

  uint m_count;

}; // class sp_instr_cpop : public sp_instr


class sp_instr_copen : public sp_instr
{
  sp_instr_copen(const sp_instr_copen &); /* Prevent use of these */
  void operator=(sp_instr_copen &);

public:

  sp_instr_copen(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {}

  virtual ~sp_instr_copen()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_cursor;		// Stack index

}; // class sp_instr_copen : public sp_instr_stmt


class sp_instr_cclose : public sp_instr
{
  sp_instr_cclose(const sp_instr_cclose &); /* Prevent use of these */
  void operator=(sp_instr_cclose &);

public:

  sp_instr_cclose(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {}

  virtual ~sp_instr_cclose()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_cursor;

}; // class sp_instr_cclose : public sp_instr


class sp_instr_cfetch : public sp_instr
{
  sp_instr_cfetch(const sp_instr_cfetch &); /* Prevent use of these */
  void operator=(sp_instr_cfetch &);

public:

  sp_instr_cfetch(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {
    m_varlist.empty();
  }

  virtual ~sp_instr_cfetch()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  void add_to_varlist(struct sp_pvar *var)
  {
    m_varlist.push_back(var);
  }

private:

  uint m_cursor;
  List<struct sp_pvar> m_varlist;

}; // class sp_instr_cfetch : public sp_instr


class sp_instr_error : public sp_instr
{
  sp_instr_error(const sp_instr_error &); /* Prevent use of these */
  void operator=(sp_instr_error &);

public:

  sp_instr_error(uint ip, sp_pcontext *ctx, int errcode)
    : sp_instr(ip, ctx), m_errcode(errcode)
  {}

  virtual ~sp_instr_error()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return UINT_MAX;
  }

private:

  int m_errcode;

}; // class sp_instr_error : public sp_instr


struct st_sp_security_context
{
  bool changed;
  uint master_access;
  uint db_access;
  char *priv_user;
  char priv_host[MAX_HOSTNAME];
  char *user;
  char *host;
  char *ip;
};

#ifndef NO_EMBEDDED_ACCESS_CHECKS
void
sp_change_security_context(THD *thd, sp_head *sp, st_sp_security_context *ctxp);
void
sp_restore_security_context(THD *thd, sp_head *sp,st_sp_security_context *ctxp);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

TABLE_LIST *
sp_add_to_query_tables(THD *thd, LEX *lex,
		       const char *db, const char *name,
		       thr_lock_type locktype);
bool
sp_add_sp_tables_to_table_list(THD *thd, LEX *lex, LEX *func_lex);

#endif /* _SP_HEAD_H_ */
