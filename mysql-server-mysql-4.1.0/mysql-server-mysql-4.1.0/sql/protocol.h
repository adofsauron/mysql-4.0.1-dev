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

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#define PACKET_BUFFET_EXTRA_ALLOC	1024

class i_string;
class THD;
#ifdef EMBEDDED_LIBRARY
typedef struct st_mysql_field MYSQL_FIELD;
#endif
class Protocol
{
protected:
  THD	 *thd;
  String *packet;
  String convert;
  uint field_pos;
#ifndef DEBUG_OFF
  enum enum_field_types *field_types;
#endif
  uint field_count;
  bool net_store_data(const char *from, uint length);
#ifdef EMBEDDED_LIBRARY
  char **next_field;
  MYSQL_FIELD *next_mysql_field;
  MEM_ROOT *alloc;
#endif
public:
  Protocol() {}
  Protocol(THD *thd) { init(thd); }
  virtual ~Protocol() {}
  void init(THD* thd);
  bool send_fields(List<Item> *list, uint flag);
  bool send_records_num(List<Item> *list, ulonglong records);
  bool store(I_List<i_string> *str_list);
  bool store(const char *from, CHARSET_INFO *cs);
  String *storage_packet() { return packet; }
  inline void free() { packet->free(); }
  bool write();
  inline  bool store(uint32 from)
  { return store_long((longlong) from); }
  inline  bool store(longlong from)
  { return store_longlong((longlong) from, 0); }
  inline  bool store(ulonglong from)
  { return store_longlong((longlong) from, 1); }

  virtual bool prepare_for_send(List<Item> *item_list) 
  {
    field_count=item_list->elements;
    return 0;
  }
  virtual void prepare_for_resend()=0;

  virtual bool store_null()=0;
  virtual bool store_tiny(longlong from)=0;
  virtual bool store_short(longlong from)=0;
  virtual bool store_long(longlong from)=0;
  virtual bool store_longlong(longlong from, bool unsigned_flag)=0;
  virtual bool store(const char *from, uint length, CHARSET_INFO *cs)=0;
  virtual bool store(float from, uint32 decimals, String *buffer)=0;
  virtual bool store(double from, uint32 decimals, String *buffer)=0;
  virtual bool store(TIME *time)=0;
  virtual bool store_date(TIME *time)=0;
  virtual bool store_time(TIME *time)=0;
  virtual bool store(Field *field)=0;
};


/* Class used for the old (MySQL 4.0 protocol) */

class Protocol_simple :public Protocol
{
public:
  Protocol_simple() {}
  Protocol_simple(THD *thd) :Protocol(thd) {}
  virtual void prepare_for_resend();
  virtual bool store_null();
  virtual bool store_tiny(longlong from);
  virtual bool store_short(longlong from);
  virtual bool store_long(longlong from);
  virtual bool store_longlong(longlong from, bool unsigned_flag);
  virtual bool store(const char *from, uint length, CHARSET_INFO *cs);
  virtual bool store(TIME *time);
  virtual bool store_date(TIME *time);
  virtual bool store_time(TIME *time);
  virtual bool store(float nr, uint32 decimals, String *buffer);
  virtual bool store(double from, uint32 decimals, String *buffer);
  virtual bool store(Field *field);
};


class Protocol_prep :public Protocol
{
private:
  uint bit_fields;
public:
  Protocol_prep() {}
  Protocol_prep(THD *thd) :Protocol(thd) {}
  virtual bool prepare_for_send(List<Item> *item_list);
  virtual void prepare_for_resend();
  virtual bool store_null();
  virtual bool store_tiny(longlong from);
  virtual bool store_short(longlong from);
  virtual bool store_long(longlong from);
  virtual bool store_longlong(longlong from, bool unsigned_flag);
  virtual bool store(const char *from,uint length, CHARSET_INFO *cs);
  virtual bool store(TIME *time);
  virtual bool store_date(TIME *time);
  virtual bool store_time(TIME *time);
  virtual bool store(float nr, uint32 decimals, String *buffer);
  virtual bool store(double from, uint32 decimals, String *buffer);
  virtual bool store(Field *field);
};

void send_warning(THD *thd, uint sql_errno, const char *err=0);
void net_printf(THD *thd,uint sql_errno, ...);
void send_ok(THD *thd, ha_rows affected_rows=0L, ulonglong id=0L,
	     const char *info=0);
void send_eof(THD *thd, bool no_flush=0);
void net_send_error(NET *net, uint sql_errno, const char *err);
char *net_store_length(char *packet,ulonglong length);
char *net_store_length(char *packet,uint length);
char *net_store_data(char *to,const char *from, uint length);
char *net_store_data(char *to,int32 from);
char *net_store_data(char *to,longlong from);
