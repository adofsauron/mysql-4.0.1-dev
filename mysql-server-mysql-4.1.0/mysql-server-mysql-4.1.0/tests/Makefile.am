# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA

## Process this file with automake to create Makefile.in

EXTRA_DIST =		auto_increment.res auto_increment.tst \
			function.res function.tst lock_test.pl lock_test.res \
			export.pl big_record.pl \
			fork2_test.pl fork_big.pl \
			insert_and_repair.pl \
			grant.pl grant.res test_delayed_insert.pl \
			pmail.pl mail_to_db.pl table_types.pl \
			udf_test udf_test.res myisam-big-rows.tst

#
# C Test for 4.1 protocol
#
INCLUDES =		-I$(top_srcdir)/include $(openssl_includes)
LIBS =			@CLIENT_LIBS@
LDADD =			@CLIENT_EXTRA_LDFLAGS@ ../libmysql/libmysqlclient.la
noinst_PROGRAMS =	client_test
client_test_LDADD=      $(LDADD) $(CXXLDFLAGS)
client_test_SOURCES=    client_test.c
client_test_DEPENDENCIES= $(LIBRARIES) $(pkglib_LTLIBRARIES)

# Don't update the files from bitkeeper
%::SCCS/s.%
