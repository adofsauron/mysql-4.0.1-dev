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

man_MANS =	mysql.1 isamchk.1 isamlog.1 mysql_zap.1 mysqlaccess.1 \
		mysqladmin.1 mysqld.1 mysqld_multi.1 mysqldump.1 mysqlshow.1 \
		perror.1 replace.1 mysqld_safe.1

EXTRA_DIST =	$(man_MANS)

# Don't update the files from bitkeeper
%::SCCS/s.%
