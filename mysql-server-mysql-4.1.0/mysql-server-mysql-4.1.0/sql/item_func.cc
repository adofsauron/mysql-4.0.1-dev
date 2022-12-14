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


/* This file defines all numerical functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "slave.h" // for wait_for_master_pos
#include <m_ctype.h>
#include <hash.h>
#include <time.h>
#include <ft_global.h>
#ifdef HAVE_COMPRESS
#include <zlib.h>
#endif

/* return TRUE if item is a constant */

bool
eval_const_cond(COND *cond)
{
  return ((Item_func*) cond)->val_int() ? TRUE : FALSE;
}


Item_func::Item_func(List<Item> &list):
  allowed_arg_cols(1)
{
  arg_count=list.elements;
  if ((args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
  {
    uint i=0;
    List_iterator_fast<Item> li(list);
    Item *item;

    while ((item=li++))
    {
      args[i++]= item;
      with_sum_func|=item->with_sum_func;
    }
  }
  list.empty();					// Fields are used
}


/*
  Resolve references to table column for a function and it's argument

  SYNOPSIS:
  fix_fields()
  thd		Thread object
  tables	List of all open tables involved in the query
  ref		Pointer to where this object is used.  This reference
		is used if we want to replace this object with another
		one (for example in the summary functions).

  DESCRIPTION
    Call fix_fields() for all arguments to the function.  The main intention
    is to allow all Item_field() objects to setup pointers to the table fields.

    Sets as a side effect the following class variables:
      maybe_null	Set if any argument may return NULL
      with_sum_func	Set if any of the arguments contains a sum function
      used_table_cache  Set to union of the arguments used table

      str_value.charset If this is a string function, set this to the
			character set for the first argument.
			If any argument is binary, this is set to binary

   If for any item any of the defaults are wrong, then this can
   be fixed in the fix_length_and_dec() function that is called
   after this one or by writing a specialized fix_fields() for the
   item.

  RETURN VALUES
  0	ok
  1	Got error.  Stored with my_error().
*/

bool
Item_func::fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
{
  Item **arg,**arg_end;
  char buff[STACK_BUFF_ALLOC];			// Max argument in function

  used_tables_cache=0;
  const_item_cache=1;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error if flag is set!
  if (arg_count)
  {						// Print purify happy
    for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
    {
      if ((*arg)->fix_fields(thd, tables, arg) ||
	        (*arg)->check_cols(allowed_arg_cols))
	    return 1;				/* purecov: inspected */
      if ((*arg)->maybe_null)
	    maybe_null=1;
      
      with_sum_func= with_sum_func || (*arg)->with_sum_func;
      used_tables_cache|=(*arg)->used_tables();
      const_item_cache&= (*arg)->const_item();
    }
  }
  fix_length_and_dec();
  fixed= 1;
  return 0;
}

void Item_func::set_outer_resolving()
{
  if (arg_count)
  {
    Item **arg,**arg_end;
    for (arg= args, arg_end= args+arg_count; arg != arg_end; arg++)
      (*arg)->set_outer_resolving();
  }
}

void Item_func::split_sum_func(Item **ref_pointer_array, List<Item> &fields)
{
  Item **arg, **arg_end;
  for (arg= args, arg_end= args+arg_count; arg != arg_end ; arg++)
  {
    if ((*arg)->with_sum_func && (*arg)->type() != SUM_FUNC_ITEM)
      (*arg)->split_sum_func(ref_pointer_array, fields);
    else if ((*arg)->used_tables() || (*arg)->type() == SUM_FUNC_ITEM)
    {
      uint el= fields.elements;
      fields.push_front(*arg);
      ref_pointer_array[el]= *arg;
      *arg= new Item_ref(ref_pointer_array + el, 0, (*arg)->name);
    }
  }
}


void Item_func::update_used_tables()
{
  used_tables_cache=0;
  const_item_cache=1;
  for (uint i=0 ; i < arg_count ; i++)
  {
    args[i]->update_used_tables();
    used_tables_cache|=args[i]->used_tables();
    const_item_cache&=args[i]->const_item();
  }
}


table_map Item_func::used_tables() const
{
  return used_tables_cache;
}

void Item_func::print(String *str)
{
  str->append(func_name());
  str->append('(');
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  str->append(')');
}


void Item_func::print_op(String *str)
{
  str->append('(');
  for (uint i=0 ; i < arg_count-1 ; i++)
  {
    args[i]->print(str);
    str->append(' ');
    str->append(func_name());
    str->append(' ');
  }
  args[arg_count-1]->print(str);
  str->append(')');
}

bool Item_func::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      func_name() != item_func->func_name())
    return 0;
  for (uint i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func->args[i], binary_cmp))
      return 0;
  return 1;
}

Field *Item_func::tmp_table_field(TABLE *t_arg)
{
  Field *res;
  LINT_INIT(res);

  switch (result_type()) {
  case INT_RESULT:
    if (max_length > 11)
      res= new Field_longlong(max_length, maybe_null, name, t_arg,
			      unsigned_flag);
    else
      res= new Field_long(max_length, maybe_null, name, t_arg,
			  unsigned_flag);
    break;
  case REAL_RESULT:
    res= new Field_double(max_length, maybe_null, name, t_arg, decimals);
    break;
  case STRING_RESULT:
    if (max_length > 255)
      res= new Field_blob(max_length, maybe_null, name, t_arg, charset());
    else
      res= new Field_string(max_length, maybe_null, name, t_arg, charset());
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return res;
}


String *Item_real_func::val_str(String *str)
{
  double nr=val();
  if (null_value)
    return 0; /* purecov: inspected */
  else
    str->set(nr,decimals,default_charset());
  return str;
}


String *Item_num_func::val_str(String *str)
{
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    else if (!unsigned_flag)
      str->set(nr,default_charset());
    else
      str->set((ulonglong) nr,default_charset());
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals,default_charset());
  }
  return str;
}


void Item_func::fix_num_length_and_dec()
{
  decimals=0;
  for (uint i=0 ; i < arg_count ; i++)
    set_if_bigger(decimals,args[i]->decimals);
  max_length=float_length(decimals);
}

Item *Item_func::get_tmp_table_item(THD *thd)
{
  if (!with_sum_func && !const_item())
    return new Item_field(result_field);
  return copy_or_same(thd);
}

String *Item_int_func::val_str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  else if (!unsigned_flag)
    str->set(nr,default_charset());
  else
    str->set((ulonglong) nr,default_charset());
  return str;
}

/*
  Change from REAL_RESULT (default) to INT_RESULT if both arguments are
  integers
*/

void Item_num_op::find_num_type(void)
{
  if (args[0]->result_type() == INT_RESULT &&
      args[1]->result_type() == INT_RESULT)
  {
    hybrid_type=INT_RESULT;
    unsigned_flag=args[0]->unsigned_flag | args[1]->unsigned_flag;
  }
}

String *Item_num_op::val_str(String *str)
{
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    else if (!unsigned_flag)
      str->set(nr,default_charset());
    else
      str->set((ulonglong) nr,default_charset());
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals,default_charset());
  }
  return str;
}


double Item_func_plus::val()
{
  double value=args[0]->val()+args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}

longlong Item_func_plus::val_int()
{
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int()+args[1]->val_int();
    if ((null_value=args[0]->null_value || args[1]->null_value))
      return 0;
    return value;
  }
  return (longlong) Item_func_plus::val();
}


/*
  The following function is here to allow the user to force
  subtraction of UNSIGNED BIGINT to return negative values.
*/

void Item_func_minus::fix_length_and_dec()
{
  Item_num_op::fix_length_and_dec();
  if (unsigned_flag &&
      (current_thd->variables.sql_mode & MODE_NO_UNSIGNED_SUBTRACTION))
    unsigned_flag=0;
}


double Item_func_minus::val()
{
  double value=args[0]->val() - args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value;
}

longlong Item_func_minus::val_int()
{
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int() - args[1]->val_int();
    if ((null_value=args[0]->null_value || args[1]->null_value))
      return 0;
    return value;
  }
  return (longlong) Item_func_minus::val();
}


double Item_func_mul::val()
{
  double value=args[0]->val()*args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return value;
}

longlong Item_func_mul::val_int()
{
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int()*args[1]->val_int();
    if ((null_value=args[0]->null_value || args[1]->null_value))
      return 0; /* purecov: inspected */
    return value;
  }
  return (longlong) Item_func_mul::val();
}


double Item_func_div::val()
{
  double value=args[0]->val();
  double val2=args[1]->val();
  if ((null_value= val2 == 0.0 || args[0]->null_value || args[1]->null_value))
    return 0.0;
  return value/val2;
}

longlong Item_func_div::val_int()
{
  if (hybrid_type == INT_RESULT)
  {
    longlong value=args[0]->val_int();
    longlong val2=args[1]->val_int();
    if ((null_value= val2 == 0 || args[0]->null_value || args[1]->null_value))
      return 0;
    return value/val2;
  }
  return (longlong) Item_func_div::val();
}

void Item_func_div::fix_length_and_dec()
{
  decimals=max(args[0]->decimals,args[1]->decimals)+2;
  max_length=args[0]->max_length - args[0]->decimals + decimals;
  uint tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  maybe_null=1;
}


/* Integer division */
longlong Item_func_int_div::val_int()
{
  longlong value=args[0]->val_int();
  longlong val2=args[1]->val_int();
  if ((null_value= val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0;
  return (unsigned_flag ?
	  (ulonglong) value / (ulonglong) val2 :
	  value / val2);
}


void Item_func_int_div::fix_length_and_dec()
{
  find_num_type();
  max_length=args[0]->max_length - args[0]->decimals;
  maybe_null=1;
}


double Item_func_mod::val()
{
  double value= floor(args[0]->val()+0.5);
  double val2=floor(args[1]->val()+0.5);
  if ((null_value=val2 == 0.0 || args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return fmod(value,val2);
}

longlong Item_func_mod::val_int()
{
  longlong value=  args[0]->val_int();
  longlong val2= args[1]->val_int();
  if ((null_value=val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return value % val2;
}

void Item_func_mod::fix_length_and_dec()
{
  max_length=args[1]->max_length;
  decimals=0;
  maybe_null=1;
  find_num_type();
}


double Item_func_neg::val()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return -value;
}

longlong Item_func_neg::val_int()
{
  longlong value=args[0]->val_int();
  null_value=args[0]->null_value;
  return -value;
}

void Item_func_neg::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT;
}

double Item_func_abs::val()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return fabs(value);
}

longlong Item_func_abs::val_int()
{
  longlong value=args[0]->val_int();
  null_value=args[0]->null_value;
  return value >= 0 ? value : -value;
}

void Item_func_abs::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT;
}

/* Gateway to natural LOG function */
double Item_func_ln::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  return log(value);
}

/* 
 Extended but so slower LOG function
 We have to check if all values are > zero and first one is not one
 as these are the cases then result is not a number.
*/ 
double Item_func_log::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  if (arg_count == 2)
  {
    double value2= args[1]->val();
    if ((null_value=(args[1]->null_value || value2 <= 0.0 || value == 1.0)))
      return 0.0;
    return log(value2) / log(value);
  }
  return log(value);
}

double Item_func_log2::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0;
  return log(value) / log(2.0);
}

double Item_func_log10::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value <= 0.0)))
    return 0.0; /* purecov: inspected */
  return log10(value);
}

double Item_func_exp::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0; /* purecov: inspected */
  return exp(value);
}

double Item_func_sqrt::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || value < 0)))
    return 0.0; /* purecov: inspected */
  return sqrt(value);
}

double Item_func_pow::val()
{
  double value=args[0]->val();
  double val2=args[1]->val();
  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0.0; /* purecov: inspected */
  return pow(value,val2);
}

// Trigonometric functions

double Item_func_acos::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(acos(value));
}

double Item_func_asin::val()
{
  double value=args[0]->val();
  if ((null_value=(args[0]->null_value || (value < -1.0 || value > 1.0))))
    return 0.0;
  return fix_result(asin(value));
}

double Item_func_atan::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  if (arg_count == 2)
  {
    double val2= args[1]->val();
    if ((null_value=args[1]->null_value))
      return 0.0;
    return fix_result(atan2(value,val2));
  }
  return fix_result(atan(value));
}

double Item_func_cos::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(cos(value));
}

double Item_func_sin::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(sin(value));
}

double Item_func_tan::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(tan(value));
}


// Shift-functions, same as << and >> in C/C++


longlong Item_func_shift_left::val_int()
{
  uint shift;
  ulonglong res= ((ulonglong) args[0]->val_int() <<
		  (shift=(uint) args[1]->val_int()));
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (shift < sizeof(longlong)*8 ? (longlong) res : LL(0));
}

longlong Item_func_shift_right::val_int()
{
  uint shift;
  ulonglong res= (ulonglong) args[0]->val_int() >>
    (shift=(uint) args[1]->val_int());
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (shift < sizeof(longlong)*8 ? (longlong) res : LL(0));
}


longlong Item_func_bit_neg::val_int()
{
  ulonglong res= (ulonglong) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0;
  return ~res;
}


// Conversion functions

void Item_func_integer::fix_length_and_dec()
{
  max_length=args[0]->max_length - args[0]->decimals+1;
  uint tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  decimals=0;
}

longlong Item_func_ceiling::val_int()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return (longlong) ceil(value);
}

longlong Item_func_floor::val_int()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return (longlong) floor(value);
}

void Item_func_round::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  if (args[1]->const_item())
  {
    int tmp=(int) args[1]->val_int();
    if (tmp < 0)
      decimals=0;
    else
      decimals=tmp;
  }
}

double Item_func_round::val()
{
  double value=args[0]->val();
  int dec=(int) args[1]->val_int();
  uint abs_dec=abs(dec);
  double tmp;
  /*
    tmp2 is here to avoid return the value with 80 bit precision
    This will fix that the test round(0.1,1) = round(0.1,1) is true
  */
  volatile double tmp2;

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  tmp=(abs_dec < array_elements(log_10) ?
       log_10[abs_dec] : pow(10.0,(double) abs_dec));

  if (truncate)
  {
    if (value >= 0)
      tmp2= dec < 0 ? floor(value/tmp)*tmp : floor(value*tmp)/tmp;
    else
      tmp2= dec < 0 ? ceil(value/tmp)*tmp : ceil(value*tmp)/tmp;
  }
  else
    tmp2=dec < 0 ? rint(value/tmp)*tmp : rint(value*tmp)/tmp;
  return tmp2;
}


void Item_func_rand::fix_length_and_dec()
{
  decimals=NOT_FIXED_DEC; 
  max_length=float_length(decimals);
  if (arg_count)
  {					// Only use argument once in query
    uint32 tmp= (uint32) (args[0]->val_int());
    if ((rand= (struct rand_struct*) sql_alloc(sizeof(*rand))))
      randominit(rand,(uint32) (tmp*0x10001L+55555555L),
		 (uint32) (tmp*0x10000001L));
  }
  else
  {
    THD *thd= current_thd;
    /*
      No need to send a Rand log event if seed was given eg: RAND(seed),
      as it will be replicated in the query as such.

      Save the seed only the first time RAND() is used in the query
      Once events are forwarded rather than recreated,
      the following can be skipped if inside the slave thread
    */
    thd->rand_used=1;
    thd->rand_saved_seed1=thd->rand.seed1;
    thd->rand_saved_seed2=thd->rand.seed2;
    rand= &thd->rand;
  }
}


double Item_func_rand::val()
{
  return my_rnd(rand);
}

longlong Item_func_sign::val_int()
{
  double value=args[0]->val();
  null_value=args[0]->null_value;
  return value < 0.0 ? -1 : (value > 0 ? 1 : 0);
}


double Item_func_units::val()
{
  double value=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0;
  return value*mul+add;
}


void Item_func_min_max::fix_length_and_dec()
{
  decimals=0;
  max_length=0;
  maybe_null=1;
  cmp_type=args[0]->result_type();

  for (uint i=0 ; i < arg_count ; i++)
  {
    if (max_length < args[i]->max_length)
      max_length=args[i]->max_length;
    if (decimals < args[i]->decimals)
      decimals=args[i]->decimals;
    if (!args[i]->maybe_null)
      maybe_null=0;
    cmp_type=item_cmp_type(cmp_type,args[i]->result_type());
    if (i==0)
      set_charset(args[i]->charset(), args[i]->coercibility);
    else if (set_charset(charset(), coercibility, 
			args[i]->charset(), args[i]->coercibility))
    {
      my_error(ER_WRONG_ARGUMENTS,MYF(0),func_name());
      break;
    }
  }
}


String *Item_func_min_max::val_str(String *str)
{
  switch (cmp_type) {
  case INT_RESULT:
  {
    longlong nr=val_int();
    if (null_value)
      return 0;
    else if (!unsigned_flag)
      str->set(nr,default_charset());
    else
      str->set((ulonglong) nr,default_charset());
    return str;
  }
  case REAL_RESULT:
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals,default_charset());
    return str;
  }
  case STRING_RESULT:
  {
    String *res;
    LINT_INIT(res);
    null_value=1;
    for (uint i=0; i < arg_count ; i++)
    {
      if (null_value)
      {
	res=args[i]->val_str(str);
	null_value=args[i]->null_value;
      }
      else
      {
	String *res2;
	res2= args[i]->val_str(res == str ? &tmp_value : str);
	if (res2)
	{
	  int cmp= sortcmp(res,res2,charset());
	  if ((cmp_sign < 0 ? cmp : -cmp) < 0)
	    res=res2;
	}
      }
    }
    res->set_charset(charset());
    return res;
  }
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;

  }
  return 0;					// Keep compiler happy
}


double Item_func_min_max::val()
{
  double value=0.0;
  null_value=1;
  for (uint i=0; i < arg_count ; i++)
  {
    if (null_value)
    {
      value=args[i]->val();
      null_value=args[i]->null_value;
    }
    else
    {
      double tmp=args[i]->val();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
  }
  return value;
}


longlong Item_func_min_max::val_int()
{
  longlong value=0;
  null_value=1;
  for (uint i=0; i < arg_count ; i++)
  {
    if (null_value)
    {
      value=args[i]->val_int();
      null_value=args[i]->null_value;
    }
    else
    {
      longlong tmp=args[i]->val_int();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
  }
  return value;
}


#ifdef HAVE_COMPRESS
longlong Item_func_crc32::val_int()
{
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) crc32(0L, (Bytef*)res->ptr(), res->length());
}
#endif /* HAVE_COMPRESS */


longlong Item_func_length::val_int()
{
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) res->length();
}

longlong Item_func_char_length::val_int()
{
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) res->numchars();
}

longlong Item_func_coercibility::val_int()
{
  if (args[0]->null_value)
  {
    null_value= 1;
    return 0;
  }
  null_value= 0;
  return (longlong) args[0]->coercibility;
}

longlong Item_func_locate::val_int()
{
  String *a=args[0]->val_str(&value1);
  String *b=args[1]->val_str(&value2);
  if (!a || !b)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  uint start=0;
#ifdef USE_MB
  uint start0=0;
#endif
  if (arg_count == 3)
  {
    start=(uint) args[2]->val_int()-1;
#ifdef USE_MB
    if (use_mb(a->charset()))
    {
      start0=start;
      if (!binary_cmp)
        start=a->charpos(start);
    }
#endif
    if (start > a->length() || start+b->length() > a->length())
      return 0;
  }
  if (!b->length())				// Found empty string at start
    return (longlong) (start+1);
#ifdef USE_MB
  if (use_mb(a->charset()) && !binary_cmp)
  {
    const char *ptr=a->ptr()+start;
    const char *search=b->ptr();
    const char *strend = ptr+a->length();
    const char *end=strend-b->length()+1;
    const char *search_end=search+b->length();
    register  uint32 l;
    while (ptr < end)
    {
      if (*ptr == *search)
      {
        register char *i,*j;
        i=(char*) ptr+1; j=(char*) search+1;
        while (j != search_end)
          if (*i++ != *j++) goto skipp;
        return (longlong) start0+1;
      }
  skipp:
      if ((l=my_ismbchar(a->charset(),ptr,strend)))
	ptr+=l;
      else ++ptr;
      ++start0;
    }
    return 0;
  }
#endif /* USE_MB */
  return (longlong) (binary_cmp ? a->strstr(*b,start) :
		     (a->strstr_case(*b,start)))+1;
}


longlong Item_func_field::val_int()
{
  String *field;
  if (!(field=item->val_str(&value)))
    return 0;					// -1 if null ?
  for (uint i=0 ; i < arg_count ; i++)
  {
    String *tmp_value=args[i]->val_str(&tmp);
    if (tmp_value && field->length() == tmp_value->length() &&
	!memcmp(field->ptr(),tmp_value->ptr(),tmp_value->length()))
      return (longlong) (i+1);
  }
  return 0;
}


void Item_func_field::split_sum_func(Item **ref_pointer_array,
				     List<Item> &fields)
{
  if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
    item->split_sum_func(ref_pointer_array, fields);
  else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
  {
    uint el= fields.elements;
    fields.push_front(item);
    ref_pointer_array[el]= item;
    item= new Item_ref(ref_pointer_array + el, 0, item->name);
  }
  Item_func::split_sum_func(ref_pointer_array, fields);
}


longlong Item_func_ascii::val_int()
{
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (longlong) (res->length() ? (uchar) (*res)[0] : (uchar) 0);
}

longlong Item_func_ord::val_int()
{
  String *res=args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (!res->length()) return 0;
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    register const char *str=res->ptr();
    register uint32 n=0, l=my_ismbchar(res->charset(),str,str+res->length());
    if (!l)
      return (longlong)((uchar) *str);
    while (l--)
      n=(n<<8)|(uint32)((uchar) *str++);
    return (longlong) n;
  }
#endif
  return (longlong) ((uchar) (*res)[0]);
}

	/* Search after a string in a string of strings separated by ',' */
	/* Returns number of found type >= 1 or 0 if not found */
	/* This optimizes searching in enums to bit testing! */

void Item_func_find_in_set::fix_length_and_dec()
{
  decimals=0;
  max_length=3;					// 1-999
  if (args[0]->const_item() && args[1]->type() == FIELD_ITEM)
  {
    Field *field= ((Item_field*) args[1])->field;
    if (field->real_type() == FIELD_TYPE_SET)
    {
      String *find=args[0]->val_str(&value);
      if (find)
      {
	enum_value=find_enum(((Field_enum*) field)->typelib,find->ptr(),
			     find->length());
	enum_bit=0;
	if (enum_value)
	  enum_bit=LL(1) << (enum_value-1);
      }
    }
  }
}

static const char separator=',';

longlong Item_func_find_in_set::val_int()
{
  if (enum_value)
  {
    ulonglong tmp=(ulonglong) args[1]->val_int();
    if (!(null_value=args[1]->null_value || args[0]->null_value))
    {
      if (tmp & enum_bit)
	return enum_value;
    }
    return 0L;
  }

  String *find=args[0]->val_str(&value);
  String *buffer=args[1]->val_str(&value2);
  if (!find || !buffer)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;

  int diff;
  CHARSET_INFO *charset= find->charset();
  if ((diff=buffer->length() - find->length()) >= 0)
  {
    const char *f_pos=find->ptr();
    const char *f_end=f_pos+find->length();
    const char *str=buffer->ptr();
    const char *end=str+diff+1;
    const char *real_end=str+buffer->length();
    uint position=1;
    do
    {
      const char *pos= f_pos;
      while (pos != f_end)
      {
	if (my_toupper(charset,*str) != my_toupper(charset,*pos))
	  goto not_found;
	str++;
	pos++;
      }
      if (str == real_end || str[0] == separator)
	return (longlong) position;
  not_found:
      while (str < end && str[0] != separator)
	str++;
      position++;
    } while (++str <= end);
  }
  return 0;
}

static char nbits[256] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

uint count_bits(ulonglong v)
{
#if SIZEOF_LONG_LONG > 4
  /* The following code is a bit faster on 16 bit machines than if we would
     only shift v */
  ulong v2=(ulong) (v >> 32);
  return (uint) (uchar) (nbits[(uchar)  v] +
                         nbits[(uchar) (v >> 8)] +
                         nbits[(uchar) (v >> 16)] +
                         nbits[(uchar) (v >> 24)] +
                         nbits[(uchar) (v2)] +
                         nbits[(uchar) (v2 >> 8)] +
                         nbits[(uchar) (v2 >> 16)] +
                         nbits[(uchar) (v2 >> 24)]);
#else
  return (uint) (uchar) (nbits[(uchar)  v] +
                         nbits[(uchar) (v >> 8)] +
                         nbits[(uchar) (v >> 16)] +
                         nbits[(uchar) (v >> 24)]);
#endif
}

longlong Item_func_bit_count::val_int()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  return (longlong) count_bits(value);
}


/****************************************************************************
** Functions to handle dynamic loadable functions
** Original source by: Alexis Mikhailov <root@medinf.chuvashia.su>
** Rewritten by monty.
****************************************************************************/

#ifdef HAVE_DLOPEN

udf_handler::~udf_handler()
{
  if (initialized)
  {
    if (u_d->func_deinit != NULL)
    {
      void (*deinit)(UDF_INIT *) = (void (*)(UDF_INIT*))
	u_d->func_deinit;
      (*deinit)(&initid);
    }
    free_udf(u_d);
  }
  if (buffers)					// Because of bug in ecc
    delete [] buffers;
}


bool
udf_handler::fix_fields(THD *thd, TABLE_LIST *tables, Item_result_field *func,
			uint arg_count, Item **arguments)
{
  char buff[STACK_BUFF_ALLOC];			// Max argument in function
  DBUG_ENTER("Item_udf_func::fix_fields");

  if (thd)
  {
    if (check_stack_overrun(thd,buff))
      return 0;					// Fatal error flag is set!
  }
  else
    thd=current_thd;				// In WHERE / const clause
  udf_func *tmp_udf=find_udf(u_d->name.str,(uint) u_d->name.length,1);

  if (!tmp_udf)
  {
    my_printf_error(ER_CANT_FIND_UDF,ER(ER_CANT_FIND_UDF),MYF(0),u_d->name.str,
		    errno);
    DBUG_RETURN(1);
  }
  u_d=tmp_udf;
  args=arguments;

  /* Fix all arguments */
  func->maybe_null=0;
  used_tables_cache=0;
  const_item_cache=1;

  if ((f_args.arg_count=arg_count))
  {
    if (!(f_args.arg_type= (Item_result*)
	  sql_alloc(f_args.arg_count*sizeof(Item_result))))

    {
      free_udf(u_d);
      DBUG_RETURN(1);
    }
    uint i;
    Item **arg,**arg_end;
    for (i=0, arg=arguments, arg_end=arguments+arg_count;
	 arg != arg_end ;
	 arg++,i++)
    {
      if ((*arg)->fix_fields(thd, tables, arg) || (*arg)->check_cols(1))
	return 1;
      if ((*arg)->binary())
	func->set_charset(&my_charset_bin);
      if ((*arg)->maybe_null)
	func->maybe_null=1;
      func->with_sum_func= func->with_sum_func || (*arg)->with_sum_func;
      used_tables_cache|=(*arg)->used_tables();
      const_item_cache&=(*arg)->const_item();
      f_args.arg_type[i]=(*arg)->result_type();
    }
    if (!(buffers=new String[arg_count]) ||
	!(f_args.args= (char**) sql_alloc(arg_count * sizeof(char *))) ||
	!(f_args.lengths=(ulong*) sql_alloc(arg_count * sizeof(long))) ||
	!(f_args.maybe_null=(char*) sql_alloc(arg_count * sizeof(char))) ||
	!(num_buffer= (char*) sql_alloc(ALIGN_SIZE(sizeof(double))*arg_count)))
    {
      free_udf(u_d);
      DBUG_RETURN(1);
    }
  }
  func->fix_length_and_dec();
  initid.max_length=func->max_length;
  initid.maybe_null=func->maybe_null;
  initid.const_item=const_item_cache;
  initid.decimals=func->decimals;
  initid.ptr=0;

  if (u_d->func_init)
  {
    char *to=num_buffer;
    for (uint i=0; i < arg_count; i++)
    {
      f_args.args[i]=0;
      f_args.lengths[i]=arguments[i]->max_length;
      f_args.maybe_null[i]=(char) arguments[i]->maybe_null;

      switch(arguments[i]->type()) {
      case Item::STRING_ITEM:			// Constant string !
      {
	String *res=arguments[i]->val_str((String *) 0);
	if (arguments[i]->null_value)
	  continue;
	f_args.args[i]=    (char*) res->ptr();
	break;
      }
      case Item::INT_ITEM:
	*((longlong*) to) = arguments[i]->val_int();
	if (!arguments[i]->null_value)
	{
	  f_args.args[i]=to;
	  to+= ALIGN_SIZE(sizeof(longlong));
	}
	break;
      case Item::REAL_ITEM:
	*((double*) to) = arguments[i]->val();
	if (!arguments[i]->null_value)
	{
	  f_args.args[i]=to;
	  to+= ALIGN_SIZE(sizeof(double));
	}
	break;
      default:					// Skip these
	break;
      }
    }
    thd->net.last_error[0]=0;
    my_bool (*init)(UDF_INIT *, UDF_ARGS *, char *)=
      (my_bool (*)(UDF_INIT *, UDF_ARGS *,  char *))
      u_d->func_init;
    if ((error=(uchar) init(&initid, &f_args, thd->net.last_error)))
    {
      my_printf_error(ER_CANT_INITIALIZE_UDF,ER(ER_CANT_INITIALIZE_UDF),MYF(0),
		      u_d->name,thd->net.last_error);
      free_udf(u_d);
      DBUG_RETURN(1);
    }
    func->max_length=min(initid.max_length,MAX_BLOB_WIDTH);
    func->maybe_null=initid.maybe_null;
    const_item_cache=initid.const_item;
    func->decimals=min(initid.decimals,31);
  }
  initialized=1;
  if (error)
  {
    my_printf_error(ER_CANT_INITIALIZE_UDF,ER(ER_CANT_INITIALIZE_UDF),MYF(0),
		    u_d->name, ER(ER_UNKNOWN_ERROR));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


bool udf_handler::get_arguments()
{
  if (error)
    return 1;					// Got an error earlier
  char *to= num_buffer;
  uint str_count=0;
  for (uint i=0; i < f_args.arg_count; i++)
  {
    f_args.args[i]=0;
    switch (f_args.arg_type[i]) {
    case STRING_RESULT:
      {
	String *res=args[i]->val_str(&buffers[str_count++]);
	if (!(args[i]->null_value))
	{
	  f_args.args[i]=    (char*) res->ptr();
	  f_args.lengths[i]= res->length();
	  break;
	}
      }
    case INT_RESULT:
      *((longlong*) to) = args[i]->val_int();
      if (!args[i]->null_value)
      {
	f_args.args[i]=to;
	to+= ALIGN_SIZE(sizeof(longlong));
      }
      break;
    case REAL_RESULT:
      *((double*) to) = args[i]->val();
      if (!args[i]->null_value)
      {
	f_args.args[i]=to;
	to+= ALIGN_SIZE(sizeof(double));
      }
      break;
    case ROW_RESULT:
    default:
      // This case should never be choosen
      DBUG_ASSERT(0);
      break;
    }
  }
  return 0;
}

/* This returns (String*) 0 in case of NULL values */

String *udf_handler::val_str(String *str,String *save_str)
{
  uchar is_null=0;
  ulong res_length;

  if (get_arguments())
    return 0;
  char * (*func)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *)=
    (char* (*)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *))
    u_d->func;

  if ((res_length=str->alloced_length()) < MAX_FIELD_WIDTH)
  {						// This happens VERY seldom
    if (str->alloc(MAX_FIELD_WIDTH))
    {
      error=1;
      return 0;
    }
  }
  char *res=func(&initid, &f_args, (char*) str->ptr(), &res_length, &is_null,
		&error);
  if (is_null || !res || error)			// The !res is for safety
  {
    return 0;
  }
  if (res == str->ptr())
  {
    str->length(res_length);
    return str;
  }
  save_str->set(res, res_length, str->charset());
  return save_str;
}



double Item_func_udf_float::val()
{
  DBUG_ENTER("Item_func_udf_float::val");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));
  DBUG_RETURN(udf.val(&null_value));
}


String *Item_func_udf_float::val_str(String *str)
{
  double nr=val();
  if (null_value)
    return 0;					/* purecov: inspected */
  else
    str->set(nr,decimals,default_charset());
  return str;
}


longlong Item_func_udf_int::val_int()
{
  DBUG_ENTER("Item_func_udf_int::val_int");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));

  DBUG_RETURN(udf.val_int(&null_value));
}


String *Item_func_udf_int::val_str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  else if (!unsigned_flag)
    str->set(nr,default_charset());
  else
    str->set((ulonglong) nr,default_charset());
  return str;
}

/* Default max_length is max argument length */

void Item_func_udf_str::fix_length_and_dec()
{
  DBUG_ENTER("Item_func_udf_str::fix_length_and_dec");
  max_length=0;
  for (uint i = 0; i < arg_count; i++)
    set_if_bigger(max_length,args[i]->max_length);
  DBUG_VOID_RETURN;
}

String *Item_func_udf_str::val_str(String *str)
{
  String *res=udf.val_str(str,&str_value);
  null_value = !res;
  return res;
}

#else
bool udf_handler::get_arguments() { return 0; }
#endif /* HAVE_DLOPEN */

/*
** User level locks
*/

pthread_mutex_t LOCK_user_locks;
static HASH hash_user_locks;

class ULL
{
  char *key;
  uint key_length;

public:
  int count;
  bool locked;
  pthread_cond_t cond;
  pthread_t thread;
  ulong thread_id;

  ULL(const char *key_arg,uint length, ulong id) 
    :key_length(length),count(1),locked(1), thread_id(id)
  {
    key=(char*) my_memdup((byte*) key_arg,length,MYF(0));
    pthread_cond_init(&cond,NULL);
    if (key)
    {
      if (hash_insert(&hash_user_locks,(byte*) this))
      {
	my_free((gptr) key,MYF(0));
	key=0;
      }
    }
  }
  ~ULL()
  {
    if (key)
    {
      hash_delete(&hash_user_locks,(byte*) this);
      my_free((gptr) key,MYF(0));
    }
    pthread_cond_destroy(&cond);
  }
  inline bool initialized() { return key != 0; }
  friend void item_user_lock_release(ULL *ull);
  friend char *ull_get_key(const ULL *ull,uint *length,my_bool not_used);
};

char *ull_get_key(const ULL *ull,uint *length,
		  my_bool not_used __attribute__((unused)))
{
  *length=(uint) ull->key_length;
  return (char*) ull->key;
}

void item_user_lock_init(void)
{
  pthread_mutex_init(&LOCK_user_locks,MY_MUTEX_INIT_SLOW);
  hash_init(&hash_user_locks,system_charset_info,
	    16,0,0,(hash_get_key) ull_get_key,NULL,0);
}

void item_user_lock_free(void)
{
  hash_free(&hash_user_locks);
  pthread_mutex_destroy(&LOCK_user_locks);
}

void item_user_lock_release(ULL *ull)
{
  ull->locked=0;
  if (mysql_bin_log.is_open())
  {
    char buf[256];
    const char *command="DO RELEASE_LOCK(\"";
    String tmp(buf,sizeof(buf), system_charset_info);
    tmp.copy(command, strlen(command), tmp.charset());
    tmp.append(ull->key,ull->key_length);
    tmp.append("\")");
    Query_log_event qev(current_thd, tmp.ptr(), tmp.length(),1);
    qev.error_code=0; // this query is always safe to run on slave
    mysql_bin_log.write(&qev);
  }
  if (--ull->count)
    pthread_cond_signal(&ull->cond);
  else
    delete ull;
}

/*
   Wait until we are at or past the given position in the master binlog
   on the slave
 */

longlong Item_master_pos_wait::val_int()
{
  THD* thd = current_thd;
  String *log_name = args[0]->val_str(&value);
  int event_count= 0;

  null_value=0;
  if (thd->slave_thread || !log_name || !log_name->length())
  {
    null_value = 1;
    return 0;
  }
  longlong pos = (ulong)args[1]->val_int();
  longlong timeout = (arg_count==3) ? args[2]->val_int() : 0 ;
#ifdef HAVE_REPLICATION
  LOCK_ACTIVE_MI;
   if ((event_count = active_mi->rli.wait_for_pos(thd, log_name, pos, timeout)) == -2)
   {
    null_value = 1;
    event_count=0;
  }
  UNLOCK_ACTIVE_MI;
#endif
  return event_count;
}

#ifdef EXTRA_DEBUG
void debug_sync_point(const char* lock_name, uint lock_timeout)
{
  THD* thd=current_thd;
  ULL* ull;
  struct timespec abstime;
  int lock_name_len,error=0;
  lock_name_len=strlen(lock_name);
  pthread_mutex_lock(&LOCK_user_locks);

  if (thd->ull)
  {
    item_user_lock_release(thd->ull);
    thd->ull=0;
  }

  /*
    If the lock has not been aquired by some client, we do not want to
    create an entry for it, since we immediately release the lock. In
    this case, we will not be waiting, but rather, just waste CPU and
    memory on the whole deal
  */
  if (!(ull= ((ULL*) hash_search(&hash_user_locks,lock_name,
				 lock_name_len))))
  {
    pthread_mutex_unlock(&LOCK_user_locks);
    return;
  }
  ull->count++;

  /*
    Structure is now initialized.  Try to get the lock.
    Set up control struct to allow others to abort locks
  */
  thd->proc_info="User lock";
  thd->mysys_var->current_mutex= &LOCK_user_locks;
  thd->mysys_var->current_cond=  &ull->cond;

  set_timespec(abstime,lock_timeout);
  while (!thd->killed &&
	 (error=pthread_cond_timedwait(&ull->cond,&LOCK_user_locks,&abstime))
	 != ETIME && error != ETIMEDOUT && ull->locked) ;
  if (ull->locked)
  {
    if (!--ull->count)
      delete ull;				// Should never happen
  }
  else
  {
    ull->locked=1;
    ull->thread=thd->real_id;
    thd->ull=ull;
  }
  pthread_mutex_unlock(&LOCK_user_locks);
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->proc_info=0;
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond=  0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);
  pthread_mutex_lock(&LOCK_user_locks);
  if (thd->ull)
  {
    item_user_lock_release(thd->ull);
    thd->ull=0;
  }
  pthread_mutex_unlock(&LOCK_user_locks);
}

#endif

/*
  Get a user level lock. If the thread has an old lock this is first released.
  Returns 1:  Got lock
  Returns 0:  Timeout
  Returns NULL: Error
*/

longlong Item_func_get_lock::val_int()
{
  String *res=args[0]->val_str(&value);
  longlong timeout=args[1]->val_int();
  struct timespec abstime;
  THD *thd=current_thd;
  ULL *ull;
  int error=0;

  pthread_mutex_lock(&LOCK_user_locks);

  if (!res || !res->length())
  {
    pthread_mutex_unlock(&LOCK_user_locks);
    null_value=1;
    return 0;
  }
  null_value=0;

  if (thd->ull)
  {
    item_user_lock_release(thd->ull);
    thd->ull=0;
  }

  if (!(ull= ((ULL*) hash_search(&hash_user_locks,(byte*) res->ptr(),
				 res->length()))))
  {
    ull=new ULL(res->ptr(),res->length(), thd->thread_id);
    if (!ull || !ull->initialized())
    {
      delete ull;
      pthread_mutex_unlock(&LOCK_user_locks);
      null_value=1;				// Probably out of memory
      return 0;
    }
    ull->thread=thd->real_id;
    thd->ull=ull;
    pthread_mutex_unlock(&LOCK_user_locks);
    return 1;					// Got new lock
  }
  ull->count++;

  /*
    Structure is now initialized.  Try to get the lock.
    Set up control struct to allow others to abort locks.
  */
  thd->proc_info="User lock";
  thd->mysys_var->current_mutex= &LOCK_user_locks;
  thd->mysys_var->current_cond=  &ull->cond;

  set_timespec(abstime,timeout);
  while (!thd->killed &&
	 (error=pthread_cond_timedwait(&ull->cond,&LOCK_user_locks,&abstime))
	 != ETIME && error != ETIMEDOUT && error != EINVAL && ull->locked) ;
  if (thd->killed)
    error=EINTR;				// Return NULL
  if (ull->locked)
  {
    if (!--ull->count)
      delete ull;				// Should never happen
    if (error != ETIME && error != ETIMEDOUT)
    {
      error=1;
      null_value=1;				// Return NULL
    }
  }
  else
  {
    ull->locked=1;
    ull->thread=thd->real_id;
    thd->ull=ull;
    error=0;
  }
  pthread_mutex_unlock(&LOCK_user_locks);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->proc_info=0;
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond=  0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  return !error ? 1 : 0;
}


/*
  Release a user level lock.
  Return:
    1 if lock released
    0 if lock wasn't held
    (SQL) NULL if no such lock
*/

longlong Item_func_release_lock::val_int()
{
  String *res=args[0]->val_str(&value);
  ULL *ull;
  longlong result;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  result=0;
  pthread_mutex_lock(&LOCK_user_locks);
  if (!(ull= ((ULL*) hash_search(&hash_user_locks,(const byte*) res->ptr(),
				 res->length()))))
  {
    null_value=1;
  }
  else
  {
    if (ull->locked && pthread_equal(pthread_self(),ull->thread))
    {
      result=1;					// Release is ok
      item_user_lock_release(ull);
      current_thd->ull=0;
    }
  }
  pthread_mutex_unlock(&LOCK_user_locks);
  return result;
}


longlong Item_func_set_last_insert_id::val_int()
{
  longlong value=args[0]->val_int();
  current_thd->insert_id(value);
  null_value=args[0]->null_value;
  return value;
}

/* This function is just used to test speed of different functions */

longlong Item_func_benchmark::val_int()
{
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff), &my_charset_bin);
  THD *thd=current_thd;

  for (ulong loop=0 ; loop < loop_count && !thd->killed; loop++)
  {
    switch (args[0]->result_type()) {
    case REAL_RESULT:
      (void) args[0]->val();
      break;
    case INT_RESULT:
      (void) args[0]->val_int();
      break;
    case STRING_RESULT:
      (void) args[0]->val_str(&tmp);
      break;
    case ROW_RESULT:
    default:
      // This case should never be choosen
      DBUG_ASSERT(0);
      return 0;
    }
  }
  return 0;
}


#define extra_size sizeof(double)

static user_var_entry *get_variable(HASH *hash, LEX_STRING &name,
				    bool create_if_not_exists)
{
  user_var_entry *entry;

  if (!(entry = (user_var_entry*) hash_search(hash, (byte*) name.str,
					      name.length)) &&
      create_if_not_exists)
  {
    uint size=ALIGN_SIZE(sizeof(user_var_entry))+name.length+1+extra_size;
    if (!hash_inited(hash))
      return 0;
    if (!(entry = (user_var_entry*) my_malloc(size,MYF(MY_WME))))
      return 0;
    entry->name.str=(char*) entry+ ALIGN_SIZE(sizeof(user_var_entry))+
      extra_size;
    entry->name.length=name.length;
    entry->value=0;
    entry->length=0;
    entry->update_query_id=0;
    entry->used_query_id=current_thd->query_id;
    entry->type=STRING_RESULT;
    memcpy(entry->name.str, name.str, name.length+1);
    if (hash_insert(hash,(byte*) entry))
    {
      my_free((char*) entry,MYF(0));
      return 0;
    }
  }
  return entry;
}



bool Item_func_set_user_var::fix_fields(THD *thd, TABLE_LIST *tables,
					Item **ref)
{
  /* fix_fields will call Item_func_set_user_var::fix_length_and_dec */
  if (Item_func::fix_fields(thd, tables, ref) ||
      !(entry= get_variable(&thd->user_vars, name, 1)))
    return 1;
  entry->update_query_id=thd->query_id;
  return 0;
}


void
Item_func_set_user_var::fix_length_and_dec()
{
  maybe_null=args[0]->maybe_null;
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  cached_result_type=args[0]->result_type();
}

void Item_func_set_user_var::update_hash(void *ptr, uint length,
					 Item_result type,
					 CHARSET_INFO *cs,
					 enum coercion coercibility)
{
  if ((null_value=args[0]->null_value))
  {
    char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
    if (entry->value && entry->value != pos)
      my_free(entry->value,MYF(0));
    entry->value=0;
    entry->length=0;
    entry->var_charset=cs;
    entry->var_coercibility= coercibility;
  }
  else
  {
    if (length <= extra_size)
    {
      /* Save value in value struct */
      char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
      if (entry->value != pos)
      {
	if (entry->value)
	  my_free(entry->value,MYF(0));
	entry->value=pos;
      }
    }
    else
    {
      /* Allocate variable */
      if (entry->length != length)
      {
	char *pos= (char*) entry+ ALIGN_SIZE(sizeof(user_var_entry));
	if (entry->value == pos)
	  entry->value=0;
	if (!(entry->value=(char*) my_realloc(entry->value, length,
					      MYF(MY_ALLOW_ZERO_PTR))))
	  goto err;
      }
    }
    memcpy(entry->value,ptr,length);
    entry->length= length;
    entry->type=type;
    entry->var_charset=cs;
    entry->var_coercibility= coercibility;
  }
  return;

 err:
  current_thd->fatal_error();			// Probably end of memory
  null_value=1;
  return;
}


bool
Item_func_set_user_var::update()
{
  switch (cached_result_type) {
  case REAL_RESULT:
    (void) val();
    break;
  case INT_RESULT:
    (void) val_int();
    break;
  case STRING_RESULT:
  {
    char buffer[MAX_FIELD_WIDTH];
    String tmp(buffer,sizeof(buffer),&my_charset_bin);
    (void) val_str(&tmp);
    break;
  }
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return current_thd->is_fatal_error;
}


double
Item_func_set_user_var::val()
{
  double value=args[0]->val();
  update_hash((void*) &value,sizeof(value), REAL_RESULT, 
    &my_charset_bin, COER_NOCOLL);
  return value;
}

longlong
Item_func_set_user_var::val_int()
{
  longlong value=args[0]->val_int();
  update_hash((void*) &value, sizeof(longlong), INT_RESULT,
	      &my_charset_bin, COER_NOCOLL);
  return value;
}

String *
Item_func_set_user_var::val_str(String *str)
{
  String *res=args[0]->val_str(str);
  if (!res)					// Null value
    update_hash((void*) 0, 0, STRING_RESULT, &my_charset_bin, COER_NOCOLL);
  else
    update_hash((void*) res->ptr(), res->length(), STRING_RESULT,
		res->charset(), args[0]->coercibility);
  return res;
}


void Item_func_set_user_var::print(String *str)
{
  str->append('(');
  str->append(name.str,name.length);
  str->append(":=",2);
  args[0]->print(str);
  str->append(')');
}


user_var_entry *Item_func_get_user_var::get_entry()
{
  if (!var_entry  || ! var_entry->value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return var_entry;
}


String *
Item_func_get_user_var::val_str(String *str)
{
  user_var_entry *entry=get_entry();
  if (!entry)
    return NULL;
  switch (entry->type) {
  case REAL_RESULT:
    str->set(*(double*) entry->value,decimals, &my_charset_bin);
    break;
  case INT_RESULT:
    str->set(*(longlong*) entry->value, &my_charset_bin);
    break;
  case STRING_RESULT:
    if (str->copy(entry->value, entry->length, entry->var_charset))
    {
      null_value=1;
      return NULL;
    }
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    break;
  }
  return str;
}


double Item_func_get_user_var::val()
{
  user_var_entry *entry=get_entry();
  if (!entry)
    return 0.0;
  switch (entry->type) {
  case REAL_RESULT:
    return *(double*) entry->value;
  case INT_RESULT:
    return (double) *(longlong*) entry->value;
  case STRING_RESULT:
    return atof(entry->value);			// This is null terminated
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0; 
  }
  return 0.0;					// Impossible
}


longlong Item_func_get_user_var::val_int()
{
  user_var_entry *entry=get_entry();
  if (!entry)
    return LL(0);
  switch (entry->type) {
  case REAL_RESULT:
    return (longlong) *(double*) entry->value;
  case INT_RESULT:
    return *(longlong*) entry->value;
  case STRING_RESULT:
    return strtoull(entry->value,NULL,10);	// String is null terminated
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    return 0;
  }
  return LL(0);					// Impossible
}


void Item_func_get_user_var::fix_length_and_dec()
{
  BINLOG_USER_VAR_EVENT *user_var_event;
  THD *thd=current_thd;
  maybe_null=1;
  decimals=NOT_FIXED_DEC;
  max_length=MAX_BLOB_WIDTH;

  if ((var_entry= get_variable(&thd->user_vars, name, 0)))
  {
    if (opt_bin_log && is_update_query(thd->lex.sql_command) &&
	var_entry->used_query_id != thd->query_id)
    {
      uint size;
      /*
	First we need to store value of var_entry, when the next situation
	appers:
        > set @a:=1;
	> insert into t1 values (@a), (@a:=@a+1), (@a:=@a+1);
	We have to write to binlog value @a= 1;
      */
      size= ALIGN_SIZE(sizeof(BINLOG_USER_VAR_EVENT)) + var_entry->length;      
      if (!(user_var_event= (BINLOG_USER_VAR_EVENT *) thd->alloc(size)))
        goto err;

      user_var_event->value= (char*) user_var_event +
	                     ALIGN_SIZE(sizeof(BINLOG_USER_VAR_EVENT));
      user_var_event->user_var_event= var_entry;
      user_var_event->type= var_entry->type;
      user_var_event->charset_number= var_entry->var_charset->number;
      if (!var_entry->value)
      {
	/* NULL value*/
	user_var_event->length= 0;
	user_var_event->value= 0;
      }
      else
      {
	user_var_event->length= var_entry->length;
	memcpy(user_var_event->value, var_entry->value,
	       var_entry->length);
      }
      var_entry->used_query_id= thd->query_id;
      if (insert_dynamic(&thd->user_var_events, (gptr) &user_var_event))
        goto err;
    }
  }
  return;

err:
  thd->fatal_error();
  return;
}


bool Item_func_get_user_var::const_item() const
{
  return var_entry && current_thd->query_id != var_entry->update_query_id;
}


enum Item_result Item_func_get_user_var::result_type() const
{
  user_var_entry *entry;
  if (!(entry = (user_var_entry*) hash_search(&current_thd->user_vars,
					      (byte*) name.str,
					      name.length)))
    return STRING_RESULT;
  return entry->type;
}


void Item_func_get_user_var::print(String *str)
{
  str->append('@');
  str->append(name.str,name.length);
  str->append(')');
}


bool Item_func_get_user_var::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;					// Same item is same.
  /* Check if other type is also a get_user_var() object */
  if (item->type() != FUNC_ITEM ||
      ((Item_func*) item)->func_name() != func_name())
    return 0;
  Item_func_get_user_var *other=(Item_func_get_user_var*) item;
  return (name.length == other->name.length &&
	  !memcmp(name.str, other->name.str, name.length));
}


longlong Item_func_inet_aton::val_int()
{
  uint byte_result = 0;
  ulonglong result = 0;			// We are ready for 64 bit addresses
  const char *p,* end;
  char c = '.'; // we mark c to indicate invalid IP in case length is 0
  char buff[36];

  String *s,tmp(buff,sizeof(buff),&my_charset_bin);
  if (!(s = args[0]->val_str(&tmp)))		// If null value
    goto err;
  null_value=0;

  end= (p = s->ptr()) + s->length();
  while (p < end)
  {
    c = *p++;
    int digit = (int) (c - '0');		// Assume ascii
    if (digit >= 0 && digit <= 9)
    {
      if ((byte_result = byte_result * 10 + digit) > 255)
	goto err;				// Wrong address
    }
    else if (c == '.')
    {
      result= (result << 8) + (ulonglong) byte_result;
      byte_result = 0;
    }
    else
      goto err;					// Invalid character
  }
  if (c != '.')					// IP number can't end on '.'
    return (result << 8) + (ulonglong) byte_result;

err:
  null_value=1;
  return 0;
}


void Item_func_match::init_search(bool no_order)
{
  DBUG_ENTER("Item_func_match::init_search");
  if (ft_handler)
    DBUG_VOID_RETURN;

  if (key == NO_SUCH_KEY)
    concat=new Item_func_concat_ws(new Item_string(" ",1,
						   default_charset_info),
				   fields);

  if (master)
  {
    join_key=master->join_key=join_key|master->join_key;
    master->init_search(no_order);
    ft_handler=master->ft_handler;
    join_key=master->join_key;
    DBUG_VOID_RETURN;
  }

  String *ft_tmp= 0;
  char tmp1[FT_QUERY_MAXLEN];
  String tmp2(tmp1,sizeof(tmp1),default_charset_info);

  // MATCH ... AGAINST (NULL) is meaningless, but possible
  if (!(ft_tmp=key_item()->val_str(&tmp2)))
  {
    ft_tmp= &tmp2;
    tmp2.set("",0,default_charset_info);
  }

  ft_handler=table->file->ft_init_ext(mode, key,
				      (byte*) ft_tmp->ptr(),
				      ft_tmp->length(),
				      join_key && !no_order);

  if (join_key)
    table->file->ft_handler=ft_handler;

  DBUG_VOID_RETURN;
}


bool Item_func_match::fix_fields(THD *thd, TABLE_LIST *tlist, Item **ref)
{
  List_iterator<Item> li(fields);
  Item *item;

  maybe_null=1;
  join_key=0;

  /*
    const_item is assumed in quite a bit of places, so it would be difficult
    to remove;  If it would ever to be removed, this should include
    modifications to find_best and auto_close as complement to auto_init code
    above.
   */
  if (Item_func::fix_fields(thd, tlist, ref) || !const_item())
  {
    my_error(ER_WRONG_ARGUMENTS,MYF(0),"AGAINST");
    return 1;
  }

  while ((item=li++))
  {
    if (item->fix_fields(thd, tlist, li.ref()) || item->check_cols(1))
      return 1;
    if (item->type() == Item::REF_ITEM)
      li.replace(item= *((Item_ref *)item)->ref);
    if (item->type() != Item::FIELD_ITEM || !item->used_tables())
      key=NO_SUCH_KEY;
    used_tables_cache|=item->used_tables();
  }
  /* check that all columns come from the same table */
  if (count_bits(used_tables_cache) != 1)
    key=NO_SUCH_KEY;
  const_item_cache=0;
  table=((Item_field *)fields.head())->field->table;
  table->fulltext_searched=1;
  record=table->record[0];
  if (key == NO_SUCH_KEY && mode != FT_BOOL)
  {
    my_error(ER_WRONG_ARGUMENTS,MYF(0),"MATCH");
    return 1;
  }

  return 0;
}

void Item_func_match::set_outer_resolving()
{
  Item_real_func::set_outer_resolving();
  List_iterator<Item> li(fields);
  Item *item;
  while ((item= li++))
    item->set_outer_resolving();
}

bool Item_func_match::fix_index()
{
  List_iterator_fast<Item> li(fields);
  Item_field *item;
  uint ft_to_key[MAX_KEY], ft_cnt[MAX_KEY], fts=0, keynr;
  uint max_cnt=0, mkeys=0;

  if (key == NO_SUCH_KEY)
    return 0;

  for (keynr=0 ; keynr < table->keys ; keynr++)
  {
    if ((table->key_info[keynr].flags & HA_FULLTEXT) &&
        (table->keys_in_use_for_query & (((key_map)1) << keynr)))
    {
      ft_to_key[fts]=keynr;
      ft_cnt[fts]=0;
      fts++;
    }
  }

  if (!fts)
    goto err;

  while ((item=(Item_field*)(li++)))
  {
    for (keynr=0 ; keynr < fts ; keynr++)
    {
      KEY *ft_key=&table->key_info[ft_to_key[keynr]];
      uint key_parts=ft_key->key_parts;

      for (uint part=0 ; part < key_parts ; part++)
      {
	if (item->field->eq(ft_key->key_part[part].field))
	  ft_cnt[keynr]++;
      }
    }
  }

  for (keynr=0 ; keynr < fts ; keynr++)
  {
    if (ft_cnt[keynr] > max_cnt)
    {
      mkeys=0;
      max_cnt=ft_cnt[mkeys]=ft_cnt[keynr];
      ft_to_key[mkeys]=ft_to_key[keynr];
      continue;
    }
    if (max_cnt && ft_cnt[keynr] == max_cnt)
    {
      mkeys++;
      ft_cnt[mkeys]=ft_cnt[keynr];
      ft_to_key[mkeys]=ft_to_key[keynr];
      continue;
    }
  }

  for (keynr=0 ; keynr <= mkeys ; keynr++)
  {
    // for now, partial keys won't work. SerG
    if (max_cnt < fields.elements ||
        max_cnt < table->key_info[ft_to_key[keynr]].key_parts)
      continue;

    key=ft_to_key[keynr];

    return 0;
  }

err:
  if (mode == FT_BOOL)
  {
    key=NO_SUCH_KEY;
    return 0;
  }
  my_printf_error(ER_FT_MATCHING_KEY_NOT_FOUND,
		  ER(ER_FT_MATCHING_KEY_NOT_FOUND),MYF(0));
  return 1;
}


bool Item_func_match::eq(const Item *item, bool binary_cmp) const
{
  if (item->type() != FUNC_ITEM ||
      func_name() != ((Item_func*)item)->func_name())
    return 0;

  Item_func_match *ifm=(Item_func_match*) item;

  if (key == ifm->key && table == ifm->table &&
      key_item()->eq(ifm->key_item(), binary_cmp))
    return 1;

  return 0;
}


double Item_func_match::val()
{
  DBUG_ENTER("Item_func_match::val");
  if (ft_handler == NULL)
    DBUG_RETURN(-1.0);

  if (join_key)
  {
    if (table->file->ft_handler)
      DBUG_RETURN(ft_handler->please->get_relevance(ft_handler));
    join_key=0;
  }

  if (key == NO_SUCH_KEY)
  {
    String *a= concat->val_str(&value);
    if ((null_value= (a == 0)))
      DBUG_RETURN(0);
    DBUG_RETURN(ft_handler->please->find_relevance(ft_handler,
				      (byte *)a->ptr(), a->length()));
  }
  else
    DBUG_RETURN(ft_handler->please->find_relevance(ft_handler, record, 0));
}


longlong Item_func_bit_xor::val_int()
{
  ulonglong arg1= (ulonglong) args[0]->val_int();
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return 0;
  return (longlong) (arg1 ^ arg2);
}


/***************************************************************************
  System variables
****************************************************************************/

Item *get_system_var(enum_var_type var_type, LEX_STRING name)
{
  if (!my_strcasecmp(system_charset_info, name.str, "VERSION"))
    return new Item_string("@@VERSION", server_version,
			   (uint) strlen(server_version),
			   system_charset_info);

  THD *thd=current_thd;
  Item *item;
  sys_var *var;
  char buff[MAX_SYS_VAR_LENGTH+3+8], *pos;

  if (!(var= find_sys_var(name.str, name.length)))
  {
    net_printf(thd, ER_UNKNOWN_SYSTEM_VARIABLE, name.str);
    return 0;
  }
  if (!(item=var->item(thd, var_type)))
    return 0;					// Impossible
  thd->lex.uncacheable();
  buff[0]='@';
  buff[1]='@';
  pos=buff+2;
  if (var_type == OPT_SESSION)
    pos=strmov(pos,"session.");
  else if (var_type == OPT_GLOBAL)
    pos=strmov(pos,"global.");
  memcpy(pos, var->name, var->name_length+1);
  // set_name() will allocate the name
  item->set_name(buff,(uint) (pos-buff)+var->name_length, system_charset_info);
  return item;
}


Item *get_system_var(enum_var_type var_type, const char *var_name, uint length,
		     const char *item_name)
{
  THD *thd=current_thd;
  Item *item;
  sys_var *var;

  var= find_sys_var(var_name, length);
  DBUG_ASSERT(var != 0);
  if (!(item=var->item(thd, var_type)))
    return 0;						// Impossible
  thd->lex.uncacheable();
  item->set_name(item_name, 0, system_charset_info);	// Will use original name
  return item;
}


/*
  Check a user level lock.

  SYNOPSIS:
    val_int()

  RETURN VALUES
    1		Available
    0		Already taken
    NULL	Error
*/

longlong Item_func_is_free_lock::val_int()
{
  String *res=args[0]->val_str(&value);
  THD *thd=current_thd;
  ULL *ull;
  int error=0;

  null_value=0;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }
  
  pthread_mutex_lock(&LOCK_user_locks);
  ull= (ULL*) hash_search(&hash_user_locks,(byte*) res->ptr(),
			  res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 1;
  return 0;
}

longlong Item_func_is_used_lock::val_int()
{
  String *res=args[0]->val_str(&value);
  THD *thd=current_thd;
  ULL *ull;

  null_value=1;
  if (!res || !res->length())
    return 0;
  
  pthread_mutex_lock(&LOCK_user_locks);
  ull= (ULL*) hash_search(&hash_user_locks,(byte*) res->ptr(),
			  res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 0;

  null_value=0;
  return ull->thread_id;
}



/**************************************************************************
  Spatial functions
***************************************************************************/

longlong Item_func_dimension::val_int()
{
  uint32 dim;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       args[0]->null_value ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       geom.dimension(&dim));
  return (longlong) dim;
}

longlong Item_func_numinteriorring::val_int()
{
  uint32 num;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, num_interior_ring) || 
	       geom.num_interior_ring(&num));
  return (longlong) num;
}

longlong Item_func_numgeometries::val_int()
{
  uint32 num= 0;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, num_geometries) || 
	       geom.num_geometries(&num));
  return (longlong) num;
}

longlong Item_func_numpoints::val_int()
{
  uint32 num;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       args[0]->null_value ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) ||
	       !GEOM_METHOD_PRESENT(geom, num_points) ||
	       geom.num_points(&num));
  return (longlong) num;
}


double Item_func_x::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, get_x) || 
	       geom.get_x(&res));
  return res;
}


double Item_func_y::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, get_y) || 
	       geom.get_y(&res));
  return res;
}


double Item_func_area::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb ||
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, area) || 
	       geom.area(&res));
  return res;
}


double Item_func_glength::val()
{
  double res;
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE) || 
	       !GEOM_METHOD_PRESENT(geom, length) || 
	       geom.length(&res));
  return res;
}


longlong Item_func_srid::val_int()
{
  String *swkb= args[0]->val_str(&value);
  Geometry geom;

  null_value= (!swkb || 
	       geom.create_from_wkb(swkb->ptr() + SRID_SIZE,
				    swkb->length() - SRID_SIZE));
  uint32 res= uint4korr(swkb->ptr());
  return (longlong) res;
}
