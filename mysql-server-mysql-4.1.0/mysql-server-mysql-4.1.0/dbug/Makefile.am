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

INCLUDES =		@MT_INCLUDES@ -I$(top_srcdir)/include
LDADD =			libdbug.a ../strings/libmystrings.a
pkglib_LIBRARIES =	libdbug.a
noinst_HEADERS =	dbug_long.h
libdbug_a_SOURCES =	dbug.c sanity.c
EXTRA_DIST =		example1.c example2.c example3.c \
			user.r monty.doc readme.prof \
			main.c factorial.c dbug_analyze.c

# Must be linked with libs that are not compiled yet
extra_progs: factorial dbug_analyze

factorial: main.o factorial.o
	@rm -f factorial
	$(LINK) main.o factorial.o -lmysys

dbug_analyze: dbug_analyze.o
	@rm -f dbug_analyze
	$(LINK) dbug_analyze.o -lmysys

user.t:		user.r $(NROFF_INC)
		nroff -cm user.r > $@

output1.r:	$(PROGRAM)
		factorial 1 2 3 4 5 | cat > $@

output2.r:	$(PROGRAM)
		factorial -\#t:o 2 3 | cat >$@

output3.r:	$(PROGRAM)
		factorial -\#d:t:o 3 | cat >$@

output4.r:	$(PROGRAM)
		factorial -\#d,result:o 4 | cat >$@

output5.r:	$(PROGRAM)
		factorial -\#d:f,factorial:F:L:o 3 | cat >$@

# Don't update the files from bitkeeper
%::SCCS/s.%
