noinst_PROGRAMS =	mysqltest mysql
client_sources =	$(mysqltest_SOURCES) $(mysql_SOURCES)

link_sources:
	for f in $(client_sources); do \
	  rm -f $(srcdir)/$$f; \
	  @LN_CP_F@ $(srcdir)/../../client/$$f $(srcdir)/$$f; \
	done;

DEFS = 		-DEMBEDDED_LIBRARY
INCLUDES =	@MT_INCLUDES@ -I$(top_srcdir)/include -I$(srcdir) \
		-I$(top_srcdir) -I$(top_srcdir)/client $(openssl_includes)
LIBS =		@LIBS@ @WRAPLIBS@
LDADD =		@CLIENT_EXTRA_LDFLAGS@ ../libmysqld.a @innodb_system_libs@ @bdb_libs_with_path@ @LIBDL@ $(CXXLDFLAGS)

mysqltest_SOURCES =	mysqltest.c

mysql_SOURCES = mysql.cc readline.cc completion_hash.cc \
                my_readline.h sql_string.h completion_hash.h
mysql_LDADD = @readline_link@ @TERMCAP_LIB@ $(LDADD)

clean:
	rm -f $(client_sources)

# Don't update the files from bitkeeper
%::SCCS/s.%
