/* Copyright (C) 2000-2003 MySQL AB

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
  UNION  of select's
  UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com>
*/


#include "mysql_priv.h"
#include "sql_select.h"

bool mysql_union(THD *thd, LEX *lex, select_result *result,
                 SELECT_LEX_UNIT *unit, ulong setup_tables_done_option)
{
  DBUG_ENTER("mysql_union");
  bool res;
  if (!(res= unit->prepare(thd, result, SELECT_NO_UNLOCK |
                           setup_tables_done_option, "")))
    res= unit->exec();
  if (!res && thd->cursor && thd->cursor->is_open())
  {
    thd->cursor->set_unit(unit);
  }
  else
    res|= unit->cleanup();
  DBUG_RETURN(res);
}


/***************************************************************************
** store records in temporary table for UNION
***************************************************************************/

select_union::select_union(TABLE *table_par)
  :table(table_par)
{
}

select_union::~select_union()
{
}


int select_union::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  unit= u;
  return 0;
}


bool select_union::send_data(List<Item> &values)
{
  int error= 0;
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  fill_record(thd, table->field, values, 1);
  if (thd->net.report_error)
    return 1;

  if ((error= table->file->write_row(table->record[0])))
  {
    /* create_myisam_from_heap will generate error if needed */
    if (error != HA_ERR_FOUND_DUPP_KEY && error != HA_ERR_FOUND_DUPP_UNIQUE &&
        create_myisam_from_heap(thd, table, &tmp_table_param, error, 1))
      return 1;
  }
  return 0;
}


bool select_union::send_eof()
{
  return 0;
}


bool select_union::flush()
{
  int error;
  if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
  {
    table->file->print_error(error, MYF(0));
    return 1;
  }
  return 0;
}


/*
  initialization procedures before fake_select_lex preparation()

  SYNOPSIS
    st_select_lex_unit::init_prepare_fake_select_lex()
    thd		- thread handler

  RETURN
    options of SELECT
*/

void
st_select_lex_unit::init_prepare_fake_select_lex(THD *thd) 
{
  thd->lex->current_select= fake_select_lex;
  fake_select_lex->ftfunc_list_alloc.empty();
  fake_select_lex->ftfunc_list= &fake_select_lex->ftfunc_list_alloc;
  fake_select_lex->table_list.link_in_list((byte *)&result_table_list,
					   (byte **)
					   &result_table_list.next_local);
  for (ORDER *order= (ORDER *)global_parameters->order_list.first;
       order;
       order=order->next)
  {
    (*order->item)->walk(&Item::change_context_processor,
                         (byte *) &fake_select_lex->context);
  }
}


bool st_select_lex_unit::prepare(THD *thd_arg, select_result *sel_result,
                                 ulong additional_options,
                                 const char *tmp_table_alias)
{
  SELECT_LEX *lex_select_save= thd_arg->lex->current_select;
  SELECT_LEX *sl, *first_select;
  select_result *tmp_result;
  bool is_union;
  TABLE *empty_table= 0;
  DBUG_ENTER("st_select_lex_unit::prepare");

  describe= test(additional_options & SELECT_DESCRIBE);

  /*
    result object should be reassigned even if preparing already done for
    max/min subquery (ALL/ANY optimization)
  */
  result= sel_result;

  if (prepared)
  {
    if (describe)
    {
      /* fast reinit for EXPLAIN */
      for (sl= first_select_in_union(); sl; sl= sl->next_select())
      {
	sl->join->result= result;
	select_limit_cnt= HA_POS_ERROR;
	offset_limit_cnt= 0;
	if (!sl->join->procedure &&
	    result->prepare(sl->join->fields_list, this))
	{
	  DBUG_RETURN(TRUE);
	}
	sl->join->select_options|= SELECT_DESCRIBE;
	sl->join->reinit();
      }
    }
    DBUG_RETURN(FALSE);
  }
  prepared= 1;
  res= FALSE;
  
  thd_arg->lex->current_select= sl= first_select= first_select_in_union();
  found_rows_for_union= first_select->options & OPTION_FOUND_ROWS;
  is_union= test(first_select->next_select());

  /* Global option */

  if (is_union)
  {
    if (!(tmp_result= union_result= new select_union(0)))
      goto err;
    union_result->tmp_table_param.init();
    if (describe)
      tmp_result= sel_result;
  }
  else
    tmp_result= sel_result;

  sl->context.resolve_in_select_list= TRUE;

  for (;sl; sl= sl->next_select())
  {
    bool can_skip_order_by;
    sl->options|=  SELECT_NO_UNLOCK;
    JOIN *join= new JOIN(thd_arg, sl->item_list, 
			 sl->options | thd_arg->options | additional_options,
			 tmp_result);
    /*
      setup_tables_done_option should be set only for very first SELECT,
      because it protect from secont setup_tables call for select-like non
      select commands (DELETE/INSERT/...) and they use only very first
      SELECT (for union it can be only INSERT ... SELECT).
    */
    additional_options&= ~OPTION_SETUP_TABLES_DONE;
    if (!join)
      goto err;

    thd_arg->lex->current_select= sl;

    can_skip_order_by= is_union && !(sl->braces && sl->explicit_limit);

    res= join->prepare(&sl->ref_pointer_array,
		       (TABLE_LIST*) sl->table_list.first, sl->with_wild,
		       sl->where,
                      (can_skip_order_by ? 0 : sl->order_list.elements) +
                       sl->group_list.elements,
                       can_skip_order_by ?
                       (ORDER*) 0 : (ORDER *)sl->order_list.first,
		       (ORDER*) sl->group_list.first,
		       sl->having,
		       (is_union ? (ORDER*) 0 :
                        (ORDER*) thd_arg->lex->proc_list.first),
		       sl, this);
    /* There are no * in the statement anymore (for PS) */
    sl->with_wild= 0;
    last_procedure= join->procedure;

    if ((res= (res || thd_arg->is_fatal_error)))
      goto err;
    if (sl == first_select)
    {
      /*
        We need to create an empty table object. It is used
        to create tmp_table fields in Item_type_holder.
        The main reason of this is that we can't create
        field object without table.
      */
      DBUG_ASSERT(!empty_table);
      empty_table= (TABLE*) thd->calloc(sizeof(TABLE));
      types.empty();
      List_iterator_fast<Item> it(sl->item_list);
      Item *item_tmp;
      while ((item_tmp= it++))
      {
	/* Error's in 'new' will be detected after loop */
	types.push_back(new Item_type_holder(thd_arg, item_tmp));
      }

      if (thd_arg->is_fatal_error)
	goto err; // out of memory
    }
    else
    {
      if (types.elements != sl->item_list.elements)
      {
	my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
		   ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT),MYF(0));
	goto err;
      }
      List_iterator_fast<Item> it(sl->item_list);
      List_iterator_fast<Item> tp(types);	
      Item *type, *item_tmp;
      while ((type= tp++, item_tmp= it++))
      {
        if (((Item_type_holder*)type)->join_types(thd_arg, item_tmp))
	  DBUG_RETURN(TRUE);
      }
    }
  }

  if (is_union)
  {
    /*
      Check that it was possible to aggregate
      all collations together for UNION.
    */
    List_iterator_fast<Item> tp(types);
    Query_arena *arena= thd->current_arena;
    Item *type;

    while ((type= tp++))
    {
      if (type->result_type() == STRING_RESULT &&
          type->collation.derivation == DERIVATION_NONE)
      {
        my_error(ER_CANT_AGGREGATE_NCOLLATIONS, MYF(0), "UNION");
        goto err;
      }
    }

    union_result->tmp_table_param.field_count= types.elements;
    if (!(table= create_tmp_table(thd_arg,
				  &union_result->tmp_table_param, types,
				  (ORDER*) 0, (bool) union_distinct, 1, 
				  (first_select_in_union()->options |
				   thd_arg->options |
				   TMP_TABLE_ALL_COLUMNS),
				  HA_POS_ERROR, (char *) tmp_table_alias)))
      goto err;
    table->file->extra(HA_EXTRA_WRITE_CACHE);
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    bzero((char*) &result_table_list, sizeof(result_table_list));
    result_table_list.db= (char*) "";
    result_table_list.table_name= result_table_list.alias= (char*) "union";
    result_table_list.table= table;
    union_result->set_table(table);

    thd_arg->lex->current_select= lex_select_save;
    if (!item_list.elements)
    {
      Field **field;
      Query_arena *tmp_arena,backup;
      tmp_arena= thd->change_arena_if_needed(&backup);

      for (field= table->field; *field; field++)
      {
	Item_field *item= new Item_field(*field);
	if (!item || item_list.push_back(item))
	{
          if (tmp_arena)
	    thd->restore_backup_item_arena(tmp_arena, &backup);
	  DBUG_RETURN(TRUE);
	}
      }
      if (tmp_arena)
        thd->restore_backup_item_arena(tmp_arena, &backup);
      if (arena->is_stmt_prepare_or_first_sp_execute())
      {
	/* prepare fake select to initialize it correctly */
	init_prepare_fake_select_lex(thd);
        /*
          Should be done only once (the only item_list per statement).
        */
        DBUG_ASSERT(fake_select_lex->join == 0);
	if (!(fake_select_lex->join= new JOIN(thd, item_list, thd->options,
					      result)))
	{
	  fake_select_lex->table_list.empty();
	  DBUG_RETURN(TRUE);
	}
	fake_select_lex->item_list= item_list;

	thd_arg->lex->current_select= fake_select_lex;
	res= fake_select_lex->join->
	  prepare(&fake_select_lex->ref_pointer_array,
		  (TABLE_LIST*) fake_select_lex->table_list.first,
		  0, 0,
		  fake_select_lex->order_list.elements,
		  (ORDER*) fake_select_lex->order_list.first,
		  (ORDER*) NULL, NULL,
                  (ORDER*) NULL,
		  fake_select_lex, this);
	fake_select_lex->table_list.empty();
      }
    }
    else if (!arena->is_conventional())
    {
      /*
        We're in execution of a prepared statement or stored procedure:
        reset field items to point at fields from the created temporary table.
      */
      List_iterator_fast<Item> it(item_list);
      for (Field **field= table->field; *field; field++)
      {
        Item_field *item_field= (Item_field*) it++;
        DBUG_ASSERT(item_field != 0);
        item_field->reset_field(*field);
      }
    }
  }

  thd_arg->lex->current_select= lex_select_save;

  DBUG_RETURN(res || thd_arg->is_fatal_error);

err:
  thd_arg->lex->current_select= lex_select_save;
  DBUG_RETURN(TRUE);
}


bool st_select_lex_unit::exec()
{
  SELECT_LEX *lex_select_save= thd->lex->current_select;
  SELECT_LEX *select_cursor=first_select_in_union();
  ulonglong add_rows=0;
  ha_rows examined_rows= 0;
  DBUG_ENTER("st_select_lex_unit::exec");

  if (executed && !uncacheable && !describe)
    DBUG_RETURN(FALSE);
  executed= 1;
  
  if (uncacheable || !item || !item->assigned() || describe)
  {
    if (item)
      item->reset_value_registration();
    if (optimized && item)
    {
      if (item->assigned())
      {
        item->assigned(0); // We will reinit & rexecute unit
        item->reset();
        table->file->delete_all_rows();
      }
      /* re-enabling indexes for next subselect iteration */
      if (union_distinct && table->file->enable_indexes(HA_KEY_SWITCH_ALL))
        DBUG_ASSERT(0);
    }
    for (SELECT_LEX *sl= select_cursor; sl; sl= sl->next_select())
    {
      ha_rows records_at_start= 0;
      thd->lex->current_select= sl;

      if (optimized)
	res= sl->join->reinit();
      else
      {
        set_limit(sl);
	if (sl == global_parameters || describe)
	{
	  offset_limit_cnt= 0;
	  /*
	    We can't use LIMIT at this stage if we are using ORDER BY for the
	    whole query
	  */
	  if (sl->order_list.first || describe)
	    select_limit_cnt= HA_POS_ERROR;
        }

        /*
          When using braces, SQL_CALC_FOUND_ROWS affects the whole query:
          we don't calculate found_rows() per union part.
          Otherwise, SQL_CALC_FOUND_ROWS should be done on all sub parts.
        */
        sl->join->select_options= 
          (select_limit_cnt == HA_POS_ERROR || sl->braces) ?
          sl->options & ~OPTION_FOUND_ROWS : sl->options | found_rows_for_union;
	res= sl->join->optimize();
      }
      if (!res)
      {
	records_at_start= table->file->records;
	sl->join->exec();
        if (sl == union_distinct)
	{
	  if (table->file->disable_indexes(HA_KEY_SWITCH_ALL))
	    DBUG_RETURN(TRUE);
	  table->no_keyread=1;
	}
	res= sl->join->error;
	offset_limit_cnt= sl->offset_limit ? sl->offset_limit->val_uint() : 0;
	if (!res)
	{
	  examined_rows+= thd->examined_row_count;
	  if (union_result->flush())
	  {
	    thd->lex->current_select= lex_select_save;
	    DBUG_RETURN(1);
	  }
	}
      }
      if (res)
      {
	thd->lex->current_select= lex_select_save;
	DBUG_RETURN(res);
      }
      /* Needed for the following test and for records_at_start in next loop */
      table->file->info(HA_STATUS_VARIABLE);
      if (found_rows_for_union && !sl->braces && 
          select_limit_cnt != HA_POS_ERROR)
      {
	/*
	  This is a union without braces. Remember the number of rows that
	  could also have been part of the result set.
	  We get this from the difference of between total number of possible
	  rows and actual rows added to the temporary table.
	*/
	add_rows+= (ulonglong) (thd->limit_found_rows - (ulonglong)
			      ((table->file->records -  records_at_start)));
      }
    }
  }
  optimized= 1;

  /* Send result to 'result' */
  res= TRUE;
  {
    List<Item_func_match> empty_list;
    empty_list.empty();

    if (!thd->is_fatal_error)				// Check if EOM
    {
      set_limit(global_parameters);
      init_prepare_fake_select_lex(thd);
      JOIN *join= fake_select_lex->join;
      if (!join)
      {
	/*
	  allocate JOIN for fake select only once (prevent
	  mysql_select automatic allocation)
	*/
	if (!(fake_select_lex->join= new JOIN(thd, item_list,
					      fake_select_lex->options, result)))
	{
	  fake_select_lex->table_list.empty();
	  DBUG_RETURN(TRUE);
	}

	/*
	  Fake st_select_lex should have item list for correctref_array
	  allocation.
	*/
	fake_select_lex->item_list= item_list;
      }
      else
      {
	JOIN_TAB *tab,*end;
	for (tab=join->join_tab, end=tab+join->tables ;
	     tab && tab != end ;
	     tab++)
	{
	  delete tab->select;
	  delete tab->quick;
	}
	join->init(thd, item_list, fake_select_lex->options, result);
      }
      res= mysql_select(thd, &fake_select_lex->ref_pointer_array,
			&result_table_list,
			0, item_list, NULL,
			global_parameters->order_list.elements,
			(ORDER*)global_parameters->order_list.first,
			(ORDER*) NULL, NULL, (ORDER*) NULL,
			fake_select_lex->options | SELECT_NO_UNLOCK,
			result, this, fake_select_lex);

      fake_select_lex->table_list.empty();
      if (!res)
      {
	thd->limit_found_rows = (ulonglong)table->file->records + add_rows;
        thd->examined_row_count+= examined_rows;
      }
      /*
	Mark for slow query log if any of the union parts didn't use
	indexes efficiently
      */
    }
  }
  thd->lex->current_select= lex_select_save;
  DBUG_RETURN(res);
}


bool st_select_lex_unit::cleanup()
{
  int error= 0;
  DBUG_ENTER("st_select_lex_unit::cleanup");

  if (cleaned)
  {
    DBUG_RETURN(FALSE);
  }
  cleaned= 1;

  if (union_result)
  {
    delete union_result;
    union_result=0; // Safety
    if (table)
      free_tmp_table(thd, table);
    table= 0; // Safety
  }

  for (SELECT_LEX *sl= first_select_in_union(); sl; sl= sl->next_select())
    error|= sl->cleanup();

  if (fake_select_lex)
  {
    JOIN *join;
    if ((join= fake_select_lex->join))
    {
      join->tables_list= 0;
      join->tables= 0;
    }
    error|= fake_select_lex->cleanup();
  }

  DBUG_RETURN(error);
}


void st_select_lex_unit::reinit_exec_mechanism()
{
  prepared= optimized= executed= 0;
#ifndef DBUG_OFF
  if (first_select()->next_select())
  {
    List_iterator_fast<Item> it(item_list);
    Item *field;
    while ((field= it++))
    {
      /*
	we can't cleanup here, because it broke link to temporary table field,
	but have to drop fixed flag to allow next fix_field of this field
	during re-executing
      */
      field->fixed= 0;
    }
  }
#endif
}


/*
  change select_result object of unit

  SYNOPSIS
    st_select_lex_unit::change_result()
    result	new select_result object
    old_result	old select_result object

  RETURN
    FALSE - OK
    TRUE  - error
*/

bool st_select_lex_unit::change_result(select_subselect *result,
                                       select_subselect *old_result)
{
  bool res= FALSE;
  for (SELECT_LEX *sl= first_select_in_union(); sl; sl= sl->next_select())
  {
    if (sl->join && sl->join->result == old_result)
      if (sl->join->change_result(result))
	return TRUE;
  }
  if (fake_select_lex && fake_select_lex->join)
    res= fake_select_lex->join->change_result(result);
  return (res);
}


bool st_select_lex::cleanup()
{
  bool error= FALSE;
  DBUG_ENTER("st_select_lex::cleanup()");

  if (join)
  {
    DBUG_ASSERT((st_select_lex*)join->select_lex == this);
    error= join->destroy();
    delete join;
    join= 0;
  }
  for (SELECT_LEX_UNIT *lex_unit= first_inner_unit(); lex_unit ;
       lex_unit= lex_unit->next_unit())
  {
    error= (bool) ((uint) error | (uint) lex_unit->cleanup());
  }
  DBUG_RETURN(error);
}

