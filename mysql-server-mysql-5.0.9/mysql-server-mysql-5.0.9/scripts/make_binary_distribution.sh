#!/bin/sh
# The default path should be /usr/local

# Get some info from configure
# chmod +x ./scripts/setsomevars

machine=@MACHINE_TYPE@
system=@SYSTEM_TYPE@
version=@VERSION@
export machine system version
SOURCE=`pwd` 
CP="cp -p"
MV="mv"

STRIP=1
DEBUG=0
SILENT=0
MACHINE=
TMP=/tmp
SUFFIX=""
NDBCLUSTER=

parse_arguments() {
  for arg do
    case "$arg" in
      --debug)    DEBUG=1;;
      --tmp=*)    TMP=`echo "$arg" | sed -e "s;--tmp=;;"` ;;
      --suffix=*) SUFFIX=`echo "$arg" | sed -e "s;--suffix=;;"` ;;
      --no-strip) STRIP=0 ;;
      --machine=*)  MACHINE=`echo "$arg" | sed -e "s;--machine=;;"` ;;
      --silent)   SILENT=1 ;;
      --with-ndbcluster) NDBCLUSTER=1 ;;
      *)
	echo "Unknown argument '$arg'"
	exit 1
        ;;
    esac
  done
}

parse_arguments "$@"



#make

# This should really be integrated with automake and not duplicate the
# installation list.

BASE=$TMP/my_dist$SUFFIX

if [ -d $BASE ] ; then
 rm -r -f $BASE
fi

BS=""
BIN_FILES=""
BASE_SYSTEM="any"
MYSQL_SHARE=$BASE/share/mysql

case $system in
  *netware*)
    BASE_SYSTEM="netware"
    BS=".nlm"
    MYSQL_SHARE=$BASE/share
    ;;
esac


mkdir $BASE $BASE/bin $BASE/docs \
 $BASE/include $BASE/lib $BASE/support-files $BASE/share $BASE/scripts \
 $BASE/mysql-test $BASE/mysql-test/t  $BASE/mysql-test/r \
 $BASE/mysql-test/include $BASE/mysql-test/std_data $BASE/mysql-test/lib

if [ $BASE_SYSTEM != "netware" ] ; then
 mkdir $BASE/share/mysql $BASE/tests $BASE/sql-bench $BASE/man \
  $BASE/man/man1 $BASE/data $BASE/data/mysql $BASE/data/test

 chmod o-rwx $BASE/data $BASE/data/*
fi

for i in ChangeLog \
				 Docs/mysql.info
do
  if [ -f $i ]
  then
    $CP $i $BASE/docs
  fi
done

for i in COPYING COPYING.LIB README Docs/INSTALL-BINARY \
         EXCEPTIONS-CLIENT MySQLEULA.txt LICENSE.doc README.NW
do
  if [ -f $i ]
  then
    $CP $i $BASE
  fi
done

# Non platform-specific bin files:
BIN_FILES="extra/comp_err$BS extra/replace$BS extra/perror$BS \
  extra/resolveip$BS extra/my_print_defaults$BS \
  extra/resolve_stack_dump$BS extra/mysql_waitpid$BS \
  myisam/myisamchk$BS myisam/myisampack$BS myisam/myisamlog$BS \
  myisam/myisam_ftdump$BS \
  sql/mysqld$BS sql/mysql_tzinfo_to_sql$BS \
  server-tools/instance-manager/mysqlmanager$BS \
  client/mysql$BS client/mysqlshow$BS client/mysqladmin$BS \
  client/mysqldump$BS client/mysqlimport$BS \
  client/mysqltest$BS client/mysqlcheck$BS \
  client/mysqlbinlog$BS \
  tests/mysql_client_test$BS \
  libmysqld/examples/mysql_client_test_embedded$BS \
  libmysqld/examples/mysqltest_embedded$BS \
  ";

# Platform-specific bin files:
if [ $BASE_SYSTEM = "netware" ] ; then
  BIN_FILES="$BIN_FILES \
    netware/mysqld_safe$BS netware/mysql_install_db$BS \
    netware/init_db.sql netware/test_db.sql netware/mysql_explain_log$BS \
    netware/mysqlhotcopy$BS netware/libmysql$BS netware/init_secure_db.sql \
    ";
# For all other platforms:
else
  BIN_FILES="$BIN_FILES \
    client/mysqltestmanagerc \
    client/mysqltestmanager-pwgen tools/mysqltestmanager \
    client/.libs/mysql client/.libs/mysqlshow client/.libs/mysqladmin \
    client/.libs/mysqldump client/.libs/mysqlimport \
    client/.libs/mysqltest client/.libs/mysqlcheck \
    client/.libs/mysqlbinlog client/.libs/mysqltestmanagerc \
    client/.libs/mysqltestmanager-pwgen tools/.libs/mysqltestmanager \
    tests/.libs/mysql_client_test \
    libmysqld/examples/.libs/mysql_client_test_embedded \
    libmysqld/examples/.libs/mysqltest_embedded \
  ";
fi

for i in $BIN_FILES
do
  if [ -f $i ]
  then
    $CP $i $BASE/bin
  fi
done

if [ x$STRIP = x1 ] ; then
  strip $BASE/bin/*
fi

# Copy not binary files
for i in sql/mysqld.sym.gz
do
  if [ -f $i ]
  then
    $CP $i $BASE/bin
  fi
done

if [ $BASE_SYSTEM = "netware" ] ; then
    $CP -r netware/*.pl $BASE/scripts
    $CP scripts/mysqlhotcopy $BASE/scripts/mysqlhotcopy.pl
fi

for i in \
  libmysql/.libs/libmysqlclient.a libmysql/.libs/libmysqlclient.so* \
  libmysql/libmysqlclient.* libmysql_r/.libs/libmysqlclient_r.a \
  libmysql_r/.libs/libmysqlclient_r.so* libmysql_r/libmysqlclient_r.* \
  mysys/libmysys.a strings/libmystrings.a dbug/libdbug.a \
  libmysqld/.libs/libmysqld.a libmysqld/.libs/libmysqld.so* \
  libmysqld/libmysqld.a netware/libmysql.imp
do
  if [ -f $i ]
  then
    $CP $i $BASE/lib
   fi
done

# convert the .a to .lib for NetWare
if [ $BASE_SYSTEM = "netware" ] ; then
    for i in $BASE/lib/*.a
    do
      libname=`basename $i .a`
      $MV $i $BASE/lib/$libname.lib
    done
fi

$CP config.h include/* $BASE/include
rm -f $BASE/include/Makefile* $BASE/include/*.in $BASE/include/config-win.h
if [ $BASE_SYSTEM != "netware" ] ; then
  rm -f $BASE/include/config-netware.h
fi

if [ $BASE_SYSTEM != "netware" ] ; then
  if [ -d tests ] ; then
    $CP tests/*.res tests/*.tst tests/*.pl $BASE/tests
  fi
  if [ -d man ] ; then
    $CP man/*.1 $BASE/man/man1
  fi
fi

$CP support-files/* $BASE/support-files
$CP scripts/*.sql $BASE/share

$CP -r sql/share/* $MYSQL_SHARE
rm -f $MYSQL_SHARE/Makefile* $MYSQL_SHARE/*/*.OLD

for i in mysql-test/mysql-test-run mysql-test/install_test_db \
         mysql-test/mysql-test-run.pl mysql-test/README \
	 mysql-test/valgrind.supp \
         netware/mysql_test_run.nlm netware/install_test_db.ncf
do
  if [ -f $i ]
  then
    $CP $i $BASE/mysql-test
   fi
done

$CP mysql-test/lib/*.pl  $BASE/mysql-test/lib
$CP mysql-test/lib/*.sql $BASE/mysql-test/lib
$CP mysql-test/include/*.inc $BASE/mysql-test/include
$CP mysql-test/t/*.def $BASE/mysql-test/t
$CP mysql-test/std_data/*.dat mysql-test/std_data/*.frm \
    mysql-test/std_data/*.pem mysql-test/std_data/Moscow_leap \
    mysql-test/std_data/des_key_file mysql-test/std_data/*.*001 \
    $BASE/mysql-test/std_data
$CP mysql-test/t/*.test mysql-test/t/*.disabled mysql-test/t/*.opt \
    mysql-test/t/*.slave-mi mysql-test/t/*.sh mysql-test/t/*.sql $BASE/mysql-test/t
$CP mysql-test/r/*.result mysql-test/r/*.require \
    $BASE/mysql-test/r

if [ $BASE_SYSTEM != "netware" ] ; then
  chmod a+x $BASE/bin/*
  $CP scripts/* $BASE/bin
  $BASE/bin/replace \@localstatedir\@ ./data \@bindir\@ ./bin \@scriptdir\@ ./bin \@libexecdir\@ ./bin \@sbindir\@ ./bin \@prefix\@ . \@HOSTNAME\@ @HOSTNAME@ \@pkgdatadir\@ ./support-files < $SOURCE/scripts/mysql_install_db.sh > $BASE/scripts/mysql_install_db
  $BASE/bin/replace \@prefix\@ /usr/local/mysql \@bindir\@ ./bin \@sbindir\@ ./bin \@libexecdir\@ ./bin \@MYSQLD_USER\@ @MYSQLD_USER@ \@localstatedir\@ /usr/local/mysql/data \@HOSTNAME\@ @HOSTNAME@ < $SOURCE/support-files/mysql.server.sh > $BASE/support-files/mysql.server
  $BASE/bin/replace /my/gnu/bin/hostname /bin/hostname -- $BASE/bin/mysqld_safe
  mv $BASE/support-files/binary-configure $BASE/configure
  chmod a+x $BASE/bin/* $BASE/scripts/* $BASE/support-files/mysql-* $BASE/support-files/mysql.server $BASE/configure
  $CP -r sql-bench/* $BASE/sql-bench
  rm -f $BASE/sql-bench/*.sh $BASE/sql-bench/Makefile* $BASE/lib/*.la
  rm -f $BASE/bin/*.sql
fi

rm -f $BASE/bin/Makefile* $BASE/bin/*.in $BASE/bin/*.sh $BASE/bin/mysql_install_db $BASE/bin/make_binary_distribution $BASE/bin/setsomevars $BASE/support-files/Makefile* $BASE/support-files/*.sh


#
# Copy system dependent files
#
if [ $BASE_SYSTEM = "netware" ] ; then
  echo "CREATE DATABASE mysql;" > $BASE/bin/init_db.sql
  echo "CREATE DATABASE test;" >> $BASE/bin/init_db.sql
  sh ./scripts/mysql_create_system_tables.sh real >> $BASE/bin/init_db.sql
  sh ./scripts/mysql_create_system_tables.sh test > $BASE/bin/test_db.sql
fi

#
# Remove system dependent files
#
if [ $BASE_SYSTEM = "netware" ] ; then
  rm -f $BASE/support-files/magic \
        $BASE/support-files/mysql.server \
        $BASE/support-files/mysql*.spec \
        $BASE/support-files/mysql-log-rotate \
        $BASE/support-files/binary-configure \
        $BASE/INSTALL-BINARY \
        $BASE/MySQLEULA.txt
else
    rm -f $BASE/README.NW
fi

# Make safe_mysqld a symlink to mysqld_safe for backwards portability
# To be removed in MySQL 4.1
if [ $BASE_SYSTEM != "netware" ] ; then
  (cd $BASE/bin ; ln -s mysqld_safe safe_mysqld )
fi

# Clean up if we did this from a bk tree
if [ -d $BASE/sql-bench/SCCS ] ; then 
  find $BASE/share -name SCCS -print | xargs rm -r -f
  find $BASE/sql-bench -name SCCS -print | xargs rm -r -f
fi

# NDB Cluster
if [ x$NDBCLUSTER = x1 ]; then
  ( cd ndb            ; @MAKE@ DESTDIR=$BASE/ndb-stage install )
  ( cd mysql-test/ndb ; @MAKE@ DESTDIR=$BASE/ndb-stage install )
  $CP $BASE/ndb-stage@bindir@/* $BASE/bin/.
  $CP $BASE/ndb-stage@libexecdir@/* $BASE/bin/.
  $CP $BASE/ndb-stage@pkglibdir@/* $BASE/lib/.
  $CP -r $BASE/ndb-stage@pkgincludedir@/ndb $BASE/include
  $CP -r $BASE/ndb-stage@prefix@/mysql-test/ndb $BASE/mysql-test/. || exit 1
  rm -rf $BASE/ndb-stage
fi

# Remove vendor from $system
system=`echo $system | sed -e 's/[a-z]*-\(.*\)/\1/g'`

# Map OS names to "our" OS names (eg. darwin6.8 -> osx10.2)
system=`echo $system | sed -e 's/darwin6.*/osx10.2/g'`
system=`echo $system | sed -e 's/darwin7.*/osx10.3/g'`
system=`echo $system | sed -e 's/darwin8.*/osx10.4/g'`
system=`echo $system | sed -e 's/\(aix4.3\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.1\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.2\).*/\1/g'`
system=`echo $system | sed -e 's/\(aix5.3\).*/\1/g'`
system=`echo $system | sed -e 's/osf5.1b/tru64/g'`
system=`echo $system | sed -e 's/linux-gnu/linux/g'`
system=`echo $system | sed -e 's/solaris2.\([0-9]*\)/solaris\1/g'`
system=`echo $system | sed -e 's/sco3.2v\(.*\)/openserver\1/g'`

# Use the override --machine if present
if [ -n "$MACHINE" ] ; then
  machine=$MACHINE
fi

# Change the distribution to a long descriptive name
NEW_NAME=mysql@MYSQL_SERVER_SUFFIX@-$version-$system-$machine$SUFFIX

# Print the platform name for build logs
echo "PLATFORM NAME: $system-$machine"

BASE2=$TMP/$NEW_NAME
rm -r -f $BASE2
mv $BASE $BASE2
BASE=$BASE2
#
# If we are compiling with gcc, copy libgcc.a to the distribution as libmygcc.a
#

if test "@GXX@" = "yes"
then
  cd $BASE/lib
  gcclib=`@CC@ --print-libgcc-file`
  if test $? -ne 0
  then
    print "Warning: Couldn't find libgcc.a!"
  else
    $CP $gcclib libmygcc.a
  fi
  cd $SOURCE
fi

#if we are debugging, do not do tar/gz
if [ x$DEBUG = x1 ] ; then
 exit
fi

# This is needed to prefere gnu tar instead of tar because tar can't
# always handle long filenames

PATH_DIRS=`echo $PATH | sed -e 's/^:/. /' -e 's/:$/ ./' -e 's/::/ . /g' -e 's/:/ /g' `
which_1 ()
{
  for cmd
  do
    for d in $PATH_DIRS
    do
      for file in $d/$cmd
      do
	if test -x $file -a ! -d $file
	then
	  echo $file
	  exit 0
	fi
      done
    done
  done
  exit 1
}

if [ $BASE_SYSTEM != "netware" ] ; then

  #
  # Create the result tar file
  #
  
  tar=`which_1 gnutar gtar`
  if test "$?" = "1" -o "$tar" = ""
  then
    tar=tar
  fi
  
  echo "Using $tar to create archive"
  cd $TMP
  
  OPT=cvf
  if [ x$SILENT = x1 ] ; then
    OPT=cf
  fi
  
  $tar $OPT $SOURCE/$NEW_NAME.tar $NEW_NAME
  cd $SOURCE
  echo "Compressing archive"
  rm -f $NEW_NAME.tar.gz
  gzip -9 $NEW_NAME.tar
  echo "$NEW_NAME.tar.gz created"
else

  #
  # Create a zip file for NetWare users
  #

  cd $TMP
  if test -e "$SOURCE/$NEW_NAME.zip"; then rm $SOURCE/$NEW_NAME.zip; fi
  zip -r $SOURCE/$NEW_NAME.zip $NEW_NAME
  echo "$NEW_NAME.zip created"

fi
echo "Removing temporary directory"
rm -r -f $BASE
