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


/* Function items used by mysql */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class Item_func_period_add :public Item_int_func
{
public:
  Item_func_period_add(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "period_add"; }
  void fix_length_and_dec() 
  { 
    max_length=6*default_charset()->mbmaxlen;
  }
};


class Item_func_period_diff :public Item_int_func
{
public:
  Item_func_period_diff(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "period_diff"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=6*default_charset()->mbmaxlen;
  }
};


class Item_func_to_days :public Item_int_func
{
public:
  Item_func_to_days(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "to_days"; }
  void fix_length_and_dec() 
  { 
    decimals=0; 
    max_length=6*default_charset()->mbmaxlen;
    maybe_null=1; 
  }
};


class Item_func_dayofmonth :public Item_int_func
{
public:
  Item_func_dayofmonth(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "dayofmonth"; }
  void fix_length_and_dec() 
  { 
    decimals=0; 
    max_length=2*default_charset()->mbmaxlen;
    maybe_null=1; 
  }
};


class Item_func_month :public Item_func
{
public:
  Item_func_month(Item *a) :Item_func(a) {}
  longlong val_int();
  double val() { return (double) Item_func_month::val_int(); }
  String *val_str(String *str) 
  {
    str->set(val_int(), default_charset());
    return null_value ? 0 : str;
  }
  const char *func_name() const { return "month"; }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() 
  { 
    set_charset(default_charset());
    decimals=0;
    max_length=2*default_charset()->mbmaxlen;
    maybe_null=1; 
  }
};


class Item_func_monthname :public Item_func_month
{
public:
  Item_func_monthname(Item *a) :Item_func_month(a) {}
  const char *func_name() const { return "monthname"; }
  String *val_str(String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() 
  {
    set_charset(default_charset());
    decimals=0;
    max_length=10*default_charset()->mbmaxlen;
    maybe_null=1; 
  }
};


class Item_func_dayofyear :public Item_int_func
{
public:
  Item_func_dayofyear(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "dayofyear"; }
  void fix_length_and_dec() 
  { 
    decimals=0;
    max_length=3*default_charset()->mbmaxlen;
    maybe_null=1; 
  }
};


class Item_func_hour :public Item_int_func
{
public:
  Item_func_hour(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "hour"; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=2*default_charset()->mbmaxlen;
    maybe_null=1;
  }
};


class Item_func_minute :public Item_int_func
{
public:
  Item_func_minute(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "minute"; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=2*default_charset()->mbmaxlen;
    maybe_null=1;
  }
};


class Item_func_quarter :public Item_int_func
{
public:
  Item_func_quarter(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "quarter"; }
  void fix_length_and_dec()
  { 
     decimals=0;
     max_length=1*default_charset()->mbmaxlen;
     maybe_null=1;
  }
};


class Item_func_second :public Item_int_func
{
public:
  Item_func_second(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "second"; }
  void fix_length_and_dec() 
  { 
    decimals=0;
    max_length=2*default_charset()->mbmaxlen;
    maybe_null=1;
  }
};


class Item_func_week :public Item_int_func
{
public:
  Item_func_week(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "week"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=2*default_charset()->mbmaxlen;
    maybe_null=1;
  }
};

class Item_func_yearweek :public Item_int_func
{
public:
  Item_func_yearweek(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "yearweek"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=6*default_charset()->mbmaxlen;
    maybe_null=1;
  }
};


class Item_func_year :public Item_int_func
{
public:
  Item_func_year(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "year"; }
  void fix_length_and_dec()
  { 
    decimals=0;
    max_length=4*default_charset()->mbmaxlen;
    maybe_null=1;
  }
};


class Item_func_weekday :public Item_func
{
  bool odbc_type;
public:
  Item_func_weekday(Item *a,bool type_arg)
    :Item_func(a), odbc_type(type_arg) {}
  longlong val_int();
  double val() { return (double) val_int(); }
  String *val_str(String *str)
  { 
    str->set(val_int(), default_charset());
    return null_value ? 0 : str;
  }
  const char *func_name() const { return "weekday"; }
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec()
  {
    set_charset(default_charset());
    decimals=0;
    max_length=1*default_charset()->mbmaxlen;
    maybe_null=1;
  }
};

class Item_func_dayname :public Item_func_weekday
{
 public:
  Item_func_dayname(Item *a) :Item_func_weekday(a,0) {}
  const char *func_name() const { return "dayname"; }
  String *val_str(String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() 
  { 
    set_charset(default_charset());
    decimals=0; 
    max_length=9*default_charset()->mbmaxlen;
    maybe_null=1; 
  }
};


class Item_func_unix_timestamp :public Item_int_func
{
  String value;
public:
  Item_func_unix_timestamp() :Item_int_func() {}
  Item_func_unix_timestamp(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "timestamp"; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=10*default_charset()->mbmaxlen;
  }
};


class Item_func_time_to_sec :public Item_int_func
{
public:
  Item_func_time_to_sec(Item *item) :Item_int_func(item) {}
  longlong val_int();
  const char *func_name() const { return "time_to_sec"; }
  void fix_length_and_dec()
  {
    decimals=0;
    max_length=10*default_charset()->mbmaxlen;
  }
};


/* This can't be a Item_str_func, because the val() functions are special */

class Item_date :public Item_func
{
public:
  Item_date() :Item_func() {}
  Item_date(Item *a) :Item_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  String *val_str(String *str);
  double val() { return (double) val_int(); }
  const char *func_name() const { return "date"; }
  void fix_length_and_dec()
  { 
    set_charset(default_charset());
    decimals=0;
    max_length=10*default_charset()->mbmaxlen;
  }
  int save_in_field(Field *to, bool no_conversions);
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_date(maybe_null, name, t_arg, default_charset()));
  }  
};


class Item_date_func :public Item_str_func
{
public:
  Item_date_func() :Item_str_func() {}
  Item_date_func(Item *a) :Item_str_func(a) {}
  Item_date_func(Item *a,Item *b) :Item_str_func(a,b) {}
  enum_field_types field_type() const { return MYSQL_TYPE_DATETIME; }
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return  (new Field_datetime(maybe_null, name, t_arg, default_charset()));
  }
};


class Item_func_curtime :public Item_func
{
  longlong value;
  char buff[9*2+32];
  uint buff_length;
public:
  Item_func_curtime() :Item_func() {}
  Item_func_curtime(Item *a) :Item_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  double val() { return (double) value; }
  longlong val_int() { return value; }
  String *val_str(String *str);
  const char *func_name() const { return "curtime"; }
  void fix_length_and_dec();
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_time(maybe_null, name, t_arg, default_charset()));
  }  
};


class Item_func_curdate :public Item_date
{
  longlong value;
  TIME ltime;
public:
  Item_func_curdate() :Item_date() {}
  longlong val_int() { return (value) ; }
  const char *func_name() const { return "curdate"; }
  void fix_length_and_dec();			/* Retrieves curtime */
  bool get_date(TIME *res,bool fuzzy_date);
};


class Item_func_now :public Item_date_func
{
  longlong value;
  char buff[20*2+32];	// +32 to make my_snprintf_{8bit|ucs2} happy
  uint buff_length;
  TIME ltime;
public:
  Item_func_now() :Item_date_func() {}
  Item_func_now(Item *a) :Item_date_func(a) {}
  enum Item_result result_type () const { return STRING_RESULT; }
  double val()	     { return (double) value; }
  longlong val_int() { return value; }
  int save_in_field(Field *to, bool no_conversions);
  String *val_str(String *str);
  const char *func_name() const { return "now"; }
  void fix_length_and_dec();
  bool get_date(TIME *res,bool fuzzy_date);
};


class Item_func_from_days :public Item_date
{
public:
  Item_func_from_days(Item *a) :Item_date(a) {}
  longlong val_int();
  const char *func_name() const { return "from_days"; }
};


class Item_func_date_format :public Item_str_func
{
  int fixed_length;
  const bool date_or_time;
  String value;
public:
  Item_func_date_format(Item *a,Item *b,bool date_or_time_arg)
    :Item_str_func(a,b),date_or_time(date_or_time_arg) {}
  String *val_str(String *str);
  const char *func_name() const { return "date_format"; }
  void fix_length_and_dec();
  uint format_length(const String *format);
};


class Item_func_from_unixtime :public Item_date_func
{
 public:
  Item_func_from_unixtime(Item *a) :Item_date_func(a) {}
  double val() { return (double) Item_func_from_unixtime::val_int(); }
  longlong val_int();
  String *val_str(String *str);
  const char *func_name() const { return "from_unixtime"; }
  void fix_length_and_dec()
  { 
    set_charset(default_charset());
    decimals=0;
    max_length=19*default_charset()->mbmaxlen;
  }
  bool get_date(TIME *res,bool fuzzy_date);
};


class Item_func_sec_to_time :public Item_str_func
{
public:
  Item_func_sec_to_time(Item *item) :Item_str_func(item) {}
  double val() { return (double) Item_func_sec_to_time::val_int(); }
  longlong val_int();
  String *val_str(String *);
  void fix_length_and_dec()
  { 
    set_charset(default_charset());
    maybe_null=1;
    max_length=13*default_charset()->mbmaxlen;
  }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  const char *func_name() const { return "sec_to_time"; }
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_time(maybe_null, name, t_arg, default_charset()));
  }
};

/*
  The following must be sorted so that simple intervals comes first.
  (get_interval_value() depends on this)
*/

enum interval_type
{
  INTERVAL_YEAR, INTERVAL_MONTH, INTERVAL_DAY, INTERVAL_HOUR, INTERVAL_MINUTE,
  INTERVAL_SECOND, INTERVAL_YEAR_MONTH, INTERVAL_DAY_HOUR, INTERVAL_DAY_MINUTE,
  INTERVAL_DAY_SECOND, INTERVAL_HOUR_MINUTE, INTERVAL_HOUR_SECOND,
  INTERVAL_MINUTE_SECOND
};


class Item_date_add_interval :public Item_date_func
{
  const interval_type int_type;
  String value;
  const bool date_sub_interval;
  enum_field_types cached_field_type;

public:
  Item_date_add_interval(Item *a,Item *b,interval_type type_arg,bool neg_arg)
    :Item_date_func(a,b),int_type(type_arg), date_sub_interval(neg_arg) {}
  String *val_str(String *);
  const char *func_name() const { return "date_add_interval"; }
  void fix_length_and_dec();
  enum_field_types field_type() const { return cached_field_type; }
  double val() { return (double) val_int(); }
  longlong val_int();
  bool get_date(TIME *res,bool fuzzy_date);
};


class Item_extract :public Item_int_func
{
  const interval_type int_type;
  String value;
  bool date_value;
 public:
  Item_extract(interval_type type_arg, Item *a)
    :Item_int_func(a), int_type(type_arg) {}
  longlong val_int();
  const char *func_name() const { return "extract"; }
  void fix_length_and_dec();
};


class Item_typecast :public Item_str_func
{
public:
  Item_typecast(Item *a) :Item_str_func(a) {}
  const char *func_name() const { return "char"; }
  String *val_str(String *a)
  {
    String *tmp=args[0]->val_str(a);
    null_value=args[0]->null_value;
    if (tmp)
      tmp->set_charset(charset());
    return tmp;
  }
  void fix_length_and_dec()
  {
    set_charset(default_charset());
    max_length=args[0]->max_length;
  }
  void print(String *str);
};


class Item_char_typecast :public Item_typecast
{
public:
  Item_char_typecast(Item *a) :Item_typecast(a) {}
  void fix_length_and_dec()
  {
    set_charset(default_charset());
    max_length=args[0]->max_length;
  }
};


class Item_date_typecast :public Item_typecast
{
public:
  Item_date_typecast(Item *a) :Item_typecast(a) {}
  const char *func_name() const { return "date"; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATE; }
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_date(maybe_null, name, t_arg, default_charset()));
  }  
};


class Item_time_typecast :public Item_typecast
{
public:
  Item_time_typecast(Item *a) :Item_typecast(a) {}
  const char *func_name() const { return "time"; }
  enum_field_types field_type() const { return MYSQL_TYPE_TIME; }
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_time(maybe_null, name, t_arg, default_charset()));
  }
};


class Item_datetime_typecast :public Item_typecast
{
public:
  Item_datetime_typecast(Item *a) :Item_typecast(a) {}
  const char *func_name() const { return "datetime"; }
  enum_field_types field_type() const { return MYSQL_TYPE_DATETIME; }
  Field *tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg)
  {
    return (new Field_datetime(maybe_null, name, t_arg, default_charset()));
  }
};
