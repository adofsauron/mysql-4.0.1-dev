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

/* Prototypes for the embedded version of MySQL */

#include <my_global.h>
#include <mysql_embed.h>
#include <mysql.h>
#include <mysql_version.h>
#include <mysqld_error.h>
#include <my_pthread.h>

C_MODE_START
extern void start_embedded_connection(NET * net);
extern void end_embedded_connection(NET * net);
extern void lib_connection_phase(NET *net, int phase);
extern bool lib_dispatch_command(enum enum_server_command command, NET *net,
				 const char *arg, ulong length);
C_MODE_END
