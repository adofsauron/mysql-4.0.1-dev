# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

INCLUDES =		-I$(top_srcdir)/include $(openssl_includes)
LDADD =			@CLIENT_EXTRA_LDFLAGS@ libvio.a $(openssl_libs) 
pkglib_LIBRARIES =	libvio.a
noinst_PROGRAMS	=	test-ssl test-sslserver test-sslclient
noinst_HEADERS =	
test_ssl_SOURCES =   test-ssl.c
test_ssl_LDADD =   	@CLIENT_EXTRA_LDFLAGS@ ../dbug/libdbug.a libvio.a ../mysys/libmysys.a \
			../strings/libmystrings.a libvio.a $(openssl_libs)
test_sslserver_SOURCES =   test-sslserver.c
test_sslserver_LDADD =   @CLIENT_EXTRA_LDFLAGS@	../dbug/libdbug.a libvio.a ../mysys/libmysys.a \
			../strings/libmystrings.a libvio.a $(openssl_libs)
test_sslclient_SOURCES =   test-sslclient.c
test_sslclient_LDADD =   @CLIENT_EXTRA_LDFLAGS@	../dbug/libdbug.a libvio.a ../mysys/libmysys.a \
			../strings/libmystrings.a libvio.a $(openssl_libs)
libvio_a_SOURCES =	vio.c viosocket.c viossl.c viosslfactories.c

# Don't update the files from bitkeeper
%::SCCS/s.%
