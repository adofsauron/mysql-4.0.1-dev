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


/* classes for sum functions */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#include <my_tree.h>

class Item_sum :public Item_result_field
{
public:
  enum Sumfunctype {COUNT_FUNC,COUNT_DISTINCT_FUNC,SUM_FUNC,AVG_FUNC,MIN_FUNC,
		    MAX_FUNC, UNIQUE_USERS_FUNC,STD_FUNC,VARIANCE_FUNC,SUM_BIT_FUNC,
		    UDF_SUM_FUNC, GROUP_CONCAT_FUNC };

  Item **args,*tmp_args[2];
  uint arg_count;
  bool quick_group;			/* If incremental update of fields */

  void mark_as_sum_func();
  Item_sum() : arg_count(0),quick_group(1) 
  {
    mark_as_sum_func();
  }
  Item_sum(Item *a) :quick_group(1)
  {
    arg_count=1;
    args=tmp_args;
    args[0]=a;
    mark_as_sum_func();
  }
  Item_sum( Item *a, Item *b ) :quick_group(1)
  {
    arg_count=2;
    args=tmp_args;
    args[0]=a; args[1]=b;
    mark_as_sum_func();
  }
  Item_sum(List<Item> &list);
  //Copy constructor, need to perform subselects with temporary tables
  Item_sum(THD *thd, Item_sum &item);
  ~Item_sum() { result_field=0; }

  enum Type type() const { return SUM_FUNC_ITEM; }
  virtual enum Sumfunctype sum_func () const=0;
  virtual void reset()=0;
  virtual bool add()=0;
  virtual void reset_field()=0;
  virtual void update_field(int offset)=0;
  virtual bool keep_field_type(void) const { return 0; }
  virtual void fix_length_and_dec() { maybe_null=1; null_value=1; }
  virtual const char *func_name() const { return "?"; }
  virtual Item *result_item(Field *field)
  { return new Item_field(field);}
  table_map used_tables() const { return ~(table_map) 0; } /* Not used */
  bool const_item() const { return 0; }
  bool is_null() { return null_value; }
  void update_used_tables() { }
  void make_field(Send_field *field);
  void print(String *str);
  void fix_num_length_and_dec();
  void no_rows_in_result() { reset(); }
  virtual bool setup(THD *thd) {return 0;}
  Item *get_tmp_table_item(THD *thd);
};


class Item_sum_num :public Item_sum
{
public:
  Item_sum_num() :Item_sum() {}
  Item_sum_num(Item *item_par) :Item_sum(item_par) {}
  Item_sum_num(Item *a, Item* b) :Item_sum(a,b) {}
  Item_sum_num(List<Item> &list) :Item_sum(list) {}
  Item_sum_num(THD *thd, Item_sum_num &item) :Item_sum(thd, item) {}
  bool fix_fields(THD *, TABLE_LIST *, Item **);
  longlong val_int() { return (longlong) val(); } /* Real as default */
  String *val_str(String*str);
  void reset_field();
};


class Item_sum_int :public Item_sum_num
{
  void fix_length_and_dec()
    { decimals=0; max_length=21; maybe_null=null_value=0; }

public:
  Item_sum_int(Item *item_par) :Item_sum_num(item_par) {}
  Item_sum_int(List<Item> &list) :Item_sum_num(list) {}
  Item_sum_int(THD *thd, Item_sum_int &item) :Item_sum_num(thd, item) {}
  double val() { return (double) val_int(); }
  String *val_str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
};


class Item_sum_sum :public Item_sum_num
{
  double sum;
  void fix_length_and_dec() { maybe_null=null_value=1; }

  public:
  Item_sum_sum(Item *item_par) :Item_sum_num(item_par),sum(0.0) {}
  Item_sum_sum(THD *thd, Item_sum_sum &item) 
    :Item_sum_num(thd, item), sum(item.sum) {}
  enum Sumfunctype sum_func () const {return SUM_FUNC;}
  void reset();
  bool add();
  double val();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "sum"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_sum(thd, *this); }
};


class Item_sum_count :public Item_sum_int
{
  longlong count;
  table_map used_table_cache;

  public:
  Item_sum_count(Item *item_par)
    :Item_sum_int(item_par),count(0),used_table_cache(~(table_map) 0)
  {}
  Item_sum_count(THD *thd, Item_sum_count &item)
    :Item_sum_int(thd, item), count(item.count),
     used_table_cache(item.used_table_cache)
  {}
  table_map used_tables() const { return used_table_cache; }
  bool const_item() const { return !used_table_cache; }
  enum Sumfunctype sum_func () const { return COUNT_FUNC; }
  void reset();
  void no_rows_in_result() { count=0; }
  bool add();
  void make_const(longlong count_arg) { count=count_arg; used_table_cache=0; }
  longlong val_int();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "count"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_count(thd, *this); }
};


class TMP_TABLE_PARAM;

class Item_sum_count_distinct :public Item_sum_int
{
  TABLE *table;
  table_map used_table_cache;
  bool fix_fields(THD *thd, TABLE_LIST *tables, Item **ref);
  uint32 *field_lengths;
  TMP_TABLE_PARAM *tmp_table_param;
  TREE tree_base;
  TREE *tree;
  /*
    Following is 0 normal object and pointer to original one for copy 
    (to correctly free resources)
  */
  Item_sum_count_distinct *original;

  uint key_length;
  CHARSET_INFO *key_charset;
  
  // calculated based on max_heap_table_size. If reached,
  // walk the tree and dump it into MyISAM table
  uint max_elements_in_tree;

  // the first few bytes of record ( at least one)
  // are just markers for deleted and NULLs. We want to skip them since
  // they will just bloat the tree without providing any valuable info
  int rec_offset;

  // If there are no blobs, we can use a tree, which
  // is faster than heap table. In that case, we still use the table
  // to help get things set up, but we insert nothing in it
  bool use_tree;
  bool always_null;		// Set to 1 if the result is always NULL

  int tree_to_myisam();

  friend int composite_key_cmp(void* arg, byte* key1, byte* key2);
  friend int simple_str_key_cmp(void* arg, byte* key1, byte* key2);
  friend int simple_raw_key_cmp(void* arg, byte* key1, byte* key2);
  friend int dump_leaf(byte* key, uint32 count __attribute__((unused)),
		       Item_sum_count_distinct* item);

  public:
  Item_sum_count_distinct(List<Item> &list)
    :Item_sum_int(list), table(0), used_table_cache(~(table_map) 0),
     tmp_table_param(0), tree(&tree_base), original(0), use_tree(0),
     always_null(0)
  { quick_group= 0; }
  Item_sum_count_distinct(THD *thd, Item_sum_count_distinct &item)
    :Item_sum_int(thd, item), table(item.table),
     used_table_cache(item.used_table_cache),
     field_lengths(item.field_lengths), tmp_table_param(item.tmp_table_param),
     tree(item.tree), original(&item), key_length(item.key_length),
     max_elements_in_tree(item.max_elements_in_tree),
     rec_offset(item.rec_offset), use_tree(item.use_tree),
     always_null(item.always_null)
  {}
  ~Item_sum_count_distinct();

  table_map used_tables() const { return used_table_cache; }
  enum Sumfunctype sum_func () const { return COUNT_DISTINCT_FUNC; }
  void reset();
  bool add();
  longlong val_int();
  void reset_field() { return ;}		// Never called
  void update_field(int offset) { return ; }	// Never called
  const char *func_name() const { return "count_distinct"; }
  bool setup(THD *thd);
  Item *copy_or_same(THD* thd) 
  {
    return new Item_sum_count_distinct(thd, *this);
  }
  void no_rows_in_result() {}
};


/* Item to get the value of a stored sum function */

class Item_sum_avg;

class Item_avg_field :public Item_result_field
{
public:
  Field *field;
  Item_avg_field(Item_sum_avg *item);
  enum Type type() const { return FIELD_AVG_ITEM; }
  double val();
  longlong val_int() { return (longlong) val(); }
  bool is_null() { (void) val_int(); return null_value; }
  String *val_str(String*);
  enum_field_types field_type() const { return MYSQL_TYPE_DOUBLE; }
  void fix_length_and_dec() {}
};


class Item_sum_avg :public Item_sum_num
{
  void fix_length_and_dec() { decimals+=4; maybe_null=1; }

  double sum;
  ulonglong count;

  public:
  Item_sum_avg(Item *item_par) :Item_sum_num(item_par),count(0) {}
  Item_sum_avg(THD *thd, Item_sum_avg &item)
    :Item_sum_num(thd, item), sum(item.sum), count(item.count) {}
  enum Sumfunctype sum_func () const {return AVG_FUNC;}
  void reset();
  bool add();
  double val();
  void reset_field();
  void update_field(int offset);
  Item *result_item(Field *field)
  { return new Item_avg_field(this); }
  const char *func_name() const { return "avg"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_avg(thd, *this); }
};

class Item_sum_variance;

class Item_variance_field :public Item_result_field
{
public:
  Field *field;
  Item_variance_field(Item_sum_variance *item);
  enum Type type() const {return FIELD_VARIANCE_ITEM; }
  double val();
  longlong val_int() { return (longlong) val(); }
  String *val_str(String*);
  bool is_null() { (void) val_int(); return null_value; }
  enum_field_types field_type() const { return MYSQL_TYPE_DOUBLE; }
  void fix_length_and_dec() {}
};

/*

variance(a) =

= sum (ai - avg(a))^2 / count(a) )
=  sum (ai^2 - 2*ai*avg(a) + avg(a)^2) / count(a)
=  (sum(ai^2) - sum(2*ai*avg(a)) + sum(avg(a)^2))/count(a) = 
=  (sum(ai^2) - 2*avg(a)*sum(a) + count(a)*avg(a)^2)/count(a) = 
=  (sum(ai^2) - 2*sum(a)*sum(a)/count(a) + count(a)*sum(a)^2/count(a)^2 )/count(a) = 
=  (sum(ai^2) - 2*sum(a)^2/count(a) + sum(a)^2/count(a) )/count(a) = 
=  (sum(ai^2) - sum(a)^2/count(a))/count(a)

*/

class Item_sum_variance : public Item_sum_num
{
  double sum, sum_sqr;
  ulonglong count;
  void fix_length_and_dec() { decimals+=4; maybe_null=1; }

  public:
  Item_sum_variance(Item *item_par) :Item_sum_num(item_par),count(0) {}
  Item_sum_variance(THD *thd, Item_sum_variance &item):
    Item_sum_num(thd, item), sum(item.sum), sum_sqr(item.sum_sqr),
    count(item.count) {}
  enum Sumfunctype sum_func () const { return VARIANCE_FUNC; }
  void reset();
  bool add();
  double val();
  void reset_field();
  void update_field(int offset);
  Item *result_item(Field *field)
  { return new Item_variance_field(this); }
  const char *func_name() const { return "variance"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_variance(thd, *this); }
};

class Item_sum_std;

class Item_std_field :public Item_variance_field
{
public:
  Item_std_field(Item_sum_std *item);
  enum Type type() const { return FIELD_STD_ITEM; }
  double val();
};

/*
   standard_deviation(a) = sqrt(variance(a))
*/

class Item_sum_std :public Item_sum_variance
{
  public:
  Item_sum_std(Item *item_par) :Item_sum_variance(item_par){}
  enum Sumfunctype sum_func () const { return STD_FUNC; }
  double val();
  Item *result_item(Field *field)
    { return new Item_std_field(this); }
  const char *func_name() const { return "std"; }
};

// This class is a string or number function depending on num_func

class Item_sum_hybrid :public Item_sum
{
 protected:
  String value,tmp_value;
  double sum;
  longlong sum_int;
  Item_result hybrid_type;
  enum_field_types hybrid_field_type;
  int cmp_sign;
  table_map used_table_cache;
  CHARSET_INFO *cmp_charset;

  public:
  Item_sum_hybrid(Item *item_par,int sign)
    :Item_sum(item_par), hybrid_type(INT_RESULT), cmp_sign(sign),
    used_table_cache(~(table_map) 0),
    cmp_charset(&my_charset_bin)
  {}
  Item_sum_hybrid(THD *thd, Item_sum_hybrid &item):
    Item_sum(thd, item), value(item.value), tmp_value(item.tmp_value),
    sum(item.sum), sum_int(item.sum_int), hybrid_type(item.hybrid_type),
    cmp_sign(item.cmp_sign), used_table_cache(used_table_cache),
    cmp_charset(item.cmp_charset) {}
  bool fix_fields(THD *, TABLE_LIST *, Item **);
  table_map used_tables() const { return used_table_cache; }
  bool const_item() const { return !used_table_cache; }

  void reset()
  {
    sum=0.0;
    sum_int=0;
    value.length(0);
    null_value=1;
    add();
  }
  double val();
  longlong val_int();
  void reset_field();
  String *val_str(String *);
  void make_const() { used_table_cache=0; }
  bool keep_field_type(void) const { return 1; }
  enum Item_result result_type () const { return hybrid_type; }
  enum enum_field_types field_type() const { return hybrid_field_type; }
  void update_field(int offset);
  void min_max_update_str_field(int offset);
  void min_max_update_real_field(int offset);
  void min_max_update_int_field(int offset);
};


class Item_sum_min :public Item_sum_hybrid
{
public:
  Item_sum_min(Item *item_par) :Item_sum_hybrid(item_par,1) {}
  Item_sum_min(THD *thd, Item_sum_min &item) :Item_sum_hybrid(thd, item) {}
  enum Sumfunctype sum_func () const {return MIN_FUNC;}

  bool add();
  const char *func_name() const { return "min"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_min(thd, *this); }
};


class Item_sum_max :public Item_sum_hybrid
{
public:
  Item_sum_max(Item *item_par) :Item_sum_hybrid(item_par,-1) {}
  Item_sum_max(THD *thd, Item_sum_max &item) :Item_sum_hybrid(thd, item) {}
  enum Sumfunctype sum_func () const {return MAX_FUNC;}

  bool add();
  const char *func_name() const { return "max"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_max(thd, *this); }
};


class Item_sum_bit :public Item_sum_int
{
 protected:
  ulonglong reset_bits,bits;

  public:
  Item_sum_bit(Item *item_par,ulonglong reset_arg)
    :Item_sum_int(item_par),reset_bits(reset_arg),bits(reset_arg) {}
  Item_sum_bit(THD *thd, Item_sum_bit &item):
    Item_sum_int(thd, item), reset_bits(item.reset_bits), bits(item.bits) {}
  enum Sumfunctype sum_func () const {return SUM_BIT_FUNC;}
  void reset();
  longlong val_int();
  void reset_field();
};


class Item_sum_or :public Item_sum_bit
{
  public:
  Item_sum_or(Item *item_par) :Item_sum_bit(item_par,LL(0)) {}
  Item_sum_or(THD *thd, Item_sum_or &item) :Item_sum_bit(thd, item) {}
  bool add();
  void update_field(int offset);
  const char *func_name() const { return "bit_or"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_or(thd, *this); }
};


class Item_sum_and :public Item_sum_bit
{
  public:
  Item_sum_and(Item *item_par) :Item_sum_bit(item_par, ~(ulonglong) LL(0)) {}
  Item_sum_and(THD *thd, Item_sum_and &item) :Item_sum_bit(thd, item) {}
  bool add();
  void update_field(int offset);
  const char *func_name() const { return "bit_and"; }
  Item *copy_or_same(THD* thd) { return new Item_sum_and(thd, *this); }
};

/*
**	user defined aggregates
*/
#ifdef HAVE_DLOPEN

class Item_udf_sum : public Item_sum
{
protected:
  udf_handler udf;

public:
  Item_udf_sum(udf_func *udf_arg) :Item_sum(), udf(udf_arg) { quick_group=0;}
  Item_udf_sum( udf_func *udf_arg, List<Item> &list )
    :Item_sum( list ), udf(udf_arg)
  { quick_group=0;}
  Item_udf_sum(THD *thd, Item_udf_sum &item)
    :Item_sum(thd, item), udf(item.udf) {}
  ~Item_udf_sum() {}
  const char *func_name() const { return udf.name(); }
  bool fix_fields(THD *thd, TABLE_LIST *tables, Item **ref)
  {
    fixed= 1;
    return udf.fix_fields(thd,tables,this,this->arg_count,this->args);
  }
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  virtual bool have_field_update(void) const { return 0; }

  void reset();
  bool add();
  void reset_field() {};
  void update_field(int offset_arg) {};
};


class Item_sum_udf_float :public Item_udf_sum
{
 public:
  Item_sum_udf_float(udf_func *udf_arg) :Item_udf_sum(udf_arg) {}
  Item_sum_udf_float(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg,list) {}
  Item_sum_udf_float(THD *thd, Item_sum_udf_float &item)
    :Item_udf_sum(thd, item) {}
  ~Item_sum_udf_float() {}
  longlong val_int() { return (longlong) Item_sum_udf_float::val(); }
  double val();
  String *val_str(String*str);
  void fix_length_and_dec() { fix_num_length_and_dec(); }
  Item *copy_or_same(THD* thd) { return new Item_sum_udf_float(thd, *this); }
};


class Item_sum_udf_int :public Item_udf_sum
{
public:
  Item_sum_udf_int(udf_func *udf_arg) :Item_udf_sum(udf_arg) {}
  Item_sum_udf_int(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg,list) {}
  Item_sum_udf_int(THD *thd, Item_sum_udf_int &item)
    :Item_udf_sum(thd, item) {}
  ~Item_sum_udf_int() {}
  longlong val_int();
  double val() { return (double) Item_sum_udf_int::val_int(); }
  String *val_str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
  Item *copy_or_same(THD* thd) { return new Item_sum_udf_int(thd, *this); }
};


class Item_sum_udf_str :public Item_udf_sum
{
public:
  Item_sum_udf_str(udf_func *udf_arg) :Item_udf_sum(udf_arg) {}
  Item_sum_udf_str(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg,list) {}
  Item_sum_udf_str(THD *thd, Item_sum_udf_str &item)
    :Item_udf_sum(thd, item) {}
  ~Item_sum_udf_str() {}
  String *val_str(String *);
  double val()
  {
    int err;
    String *res;  res=val_str(&str_value);
    return res ? my_strntod(res->charset(),(char*) res->ptr(),res->length(),
			    (char**) 0, &err) : 0.0;
  }
  longlong val_int()
  {
    int err;
    String *res;  res=val_str(&str_value);
    return res ? my_strntoll(res->charset(),res->ptr(),res->length(),10, (char**) 0, &err) : (longlong) 0;
  }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
  Item *copy_or_same(THD* thd) { return new Item_sum_udf_str(thd, *this); }
};

#else /* Dummy functions to get sql_yacc.cc compiled */

class Item_sum_udf_float :public Item_sum_num
{
 public:
  Item_sum_udf_float(udf_func *udf_arg) :Item_sum_num() {}
  Item_sum_udf_float(udf_func *udf_arg, List<Item> &list) :Item_sum_num() {}
  Item_sum_udf_float(THD *thd, Item_sum_udf_float &item)
    :Item_sum_num(thd, item) {}
  ~Item_sum_udf_float() {}
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  double val() { return 0.0; }
  void reset() {}
  bool add() { return 0; }  
  void update_field(int offset) {}
  Item *copy_or_same(THD* thd) { return new Item_sum_udf_float(thd, *this); }
};


class Item_sum_udf_int :public Item_sum_num
{
public:
  Item_sum_udf_int(udf_func *udf_arg) :Item_sum_num() {}
  Item_sum_udf_int(udf_func *udf_arg, List<Item> &list) :Item_sum_num() {}
  Item_sum_udf_int(THD *thd, Item_sum_udf_int &item)
    :Item_sum_num(thd, item) {}
  ~Item_sum_udf_int() {}
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  longlong val_int() { return 0; }
  double val() { return 0; }
  void reset() {}
  bool add() { return 0; }  
  void update_field(int offset) {}
  Item *copy_or_same(THD* thd) { return new Item_sum_udf_int(thd, *this); }
};


class Item_sum_udf_str :public Item_sum_num
{
public:
  Item_sum_udf_str(udf_func *udf_arg) :Item_sum_num() {}
  Item_sum_udf_str(udf_func *udf_arg, List<Item> &list)  :Item_sum_num() {}
  Item_sum_udf_str(THD *thd, Item_sum_udf_str &item)
    :Item_sum_num(thd, item) {}
  ~Item_sum_udf_str() {}
  String *val_str(String *) { null_value=1; return 0; }
  double val() { null_value=1; return 0.0; }
  longlong val_int() { null_value=1; return 0; }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() { maybe_null=1; max_length=0; }
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  void reset() {}
  bool add() { return 0; }  
  void update_field(int offset) {}
  Item *copy_or_same(THD* thd) { return new Item_sum_udf_str(thd, *this); }
};

#endif /* HAVE_DLOPEN */

class MYSQL_ERROR;

class Item_func_group_concat : public Item_sum
{
  THD *item_thd;
  TMP_TABLE_PARAM *tmp_table_param;
  uint max_elements_in_tree;
  MYSQL_ERROR *warning;
  bool warning_available;
 public:
  String result;
  String *separator;
  uint show_elements;
  TREE tree_base;
  TREE *tree;
  TABLE *table;
  int arg_count_order;
  int arg_count_field;
  int arg_show_fields;
  int distinct;
  Item **expr;
  ORDER **order;
  bool tree_mode;
  int count_cut_values;
  ulong group_concat_max_len;
  bool warning_for_row;
  TABLE_LIST *tables_list;
  bool always_null;
  /*
    Following is 0 normal object and pointer to original one for copy 
    (to correctly free resources)
  */
  Item_func_group_concat *original;
  
  Item_func_group_concat(int is_distinct,List<Item> *is_select,
                         SQL_LIST *is_order,String *is_separator);
			 
  Item_func_group_concat(THD *thd, Item_func_group_concat &item)
    :Item_sum(thd, item),item_thd(thd),
     tmp_table_param(item.tmp_table_param),
     max_elements_in_tree(item.max_elements_in_tree),
     warning(item.warning),
     warning_available(item.warning_available),
     separator(item.separator),
     show_elements(item.show_elements),
     tree(item.tree),      
     table(item.table),
     arg_count_order(item.arg_count_order),
     arg_count_field(item.arg_count_field),
     arg_show_fields(item.arg_show_fields),
     distinct(item.distinct),
     expr(item.expr),
     order(item.order),
     tree_mode(0),
     count_cut_values(item.count_cut_values),
     group_concat_max_len(item.group_concat_max_len),
     warning_for_row(item.warning_for_row),
     tables_list(item.tables_list),
     original(&item)
    {
     quick_group = 0;
    };
  ~Item_func_group_concat();
  enum Sumfunctype sum_func () const {return GROUP_CONCAT_FUNC;}
  const char *func_name() const { return "group_concat"; }
  enum Type type() const { return SUM_FUNC_ITEM; }  
  virtual Item_result result_type () const { return STRING_RESULT; }
  void reset();
  bool add();
  void reset_field();
  bool fix_fields(THD *, TABLE_LIST *, Item **);
  bool setup(THD *thd);
  virtual void update_field(int offset) {};
  double val()
  {
    String *res;  res=val_str(&str_value);
    return res ? atof(res->c_ptr()) : 0.0;
  }
  longlong val_int()
  {
    String *res;  res=val_str(&str_value);
    return res ? strtoll(res->c_ptr(),(char**) 0,10) : (longlong) 0;
  }
  String* val_str(String* str);
};
