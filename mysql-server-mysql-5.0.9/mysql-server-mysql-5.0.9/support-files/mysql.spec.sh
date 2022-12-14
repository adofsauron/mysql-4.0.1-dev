%define mysql_version		@VERSION@
# use "rpmbuild --with static" or "rpm --define '_with_static 1'" (for RPM 3.x)
# to enable static linking (off by default)
%{?_with_static:%define STATIC_BUILD 1}
%{!?_with_static:%define STATIC_BUILD 0}
%if %{STATIC_BUILD}
%define release 0
%else
%define release 0.glibc23
%endif
%define license GPL
%define mysqld_user		mysql
%define server_suffix -standard
%define mysqldatadir /var/lib/mysql

# We don't package all files installed into the build root by intention -
# See BUG#998 for details.
%define _unpackaged_files_terminate_build 0

%define see_base For a description of MySQL see the base MySQL RPM or http://www.mysql.com

Name: MySQL
Summary:	MySQL: a very fast and reliable SQL database server
Group:		Applications/Databases
Version:	@MYSQL_NO_DASH_VERSION@
Release:	%{release}
License:	%{license}
Source:		http://www.mysql.com/Downloads/MySQL-@MYSQL_BASE_VERSION@/mysql-%{mysql_version}.tar.gz
URL:		http://www.mysql.com/
Packager:	Lenz Grimmer <build@mysql.com>
Vendor:		MySQL AB
Requires: fileutils sh-utils
Provides:	msqlormysql MySQL-server mysql
BuildRequires: ncurses-devel
Obsoletes:	mysql

# Think about what you use here since the first step is to
# run a rm -rf
BuildRoot:    %{_tmppath}/%{name}-%{version}-build

# From the manual
%description
The MySQL(TM) software delivers a very fast, multi-threaded, multi-user,
and robust SQL (Structured Query Language) database server. MySQL Server
is intended for mission-critical, heavy-load production systems as well
as for embedding into mass-deployed software. MySQL is a trademark of
MySQL AB.

The MySQL software has Dual Licensing, which means you can use the MySQL
software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from MySQL AB if you do not wish to be bound by the terms of
the GPL. See the chapter "Licensing and Support" in the manual for
further info.

The MySQL web site (http://www.mysql.com/) provides the latest
news and information about the MySQL software. Also please see the
documentation and the manual for more information.

%package server
Summary:	MySQL: a very fast and reliable SQL database server
Group:		Applications/Databases
Requires: fileutils sh-utils
Provides:	msqlormysql mysql-server mysql MySQL
Obsoletes:	MySQL mysql mysql-server

%description server
The MySQL(TM) software delivers a very fast, multi-threaded, multi-user,
and robust SQL (Structured Query Language) database server. MySQL Server
is intended for mission-critical, heavy-load production systems as well
as for embedding into mass-deployed software. MySQL is a trademark of
MySQL AB.

The MySQL software has Dual Licensing, which means you can use the MySQL
software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from MySQL AB if you do not wish to be bound by the terms of
the GPL. See the chapter "Licensing and Support" in the manual for
further info.

The MySQL web site (http://www.mysql.com/) provides the latest
news and information about the MySQL software. Also please see the
documentation and the manual for more information.

This package includes the MySQL server binary (incl. InnoDB) as well
as related utilities to run and administrate a MySQL server.

If you want to access and work with the database, you have to install
package "MySQL-client" as well!

%package client
Summary: MySQL - Client
Group: Applications/Databases
Obsoletes: mysql-client
Provides: mysql-client

%description client
This package contains the standard MySQL clients and administration tools. 

%{see_base}

%package ndb-storage
Summary:	MySQL - ndbcluster storage engine
Group:		Applications/Databases

%description ndb-storage
This package contains the ndbcluster storage engine. 
It is necessary to have this package installed on all 
computers that should store ndbcluster table data.
Note that this storage engine can only be used in conjunction
with the MySQL Max server.

%{see_base}

%package ndb-management
Summary:	MySQL - ndbcluster storage engine management
Group:		Applications/Databases

%description ndb-management
This package contains ndbcluster storage engine management.
It is necessary to have this package installed on at least 
one computer in the cluster.

%{see_base}

%package ndb-tools
Summary:	MySQL - ndbcluster storage engine basic tools
Group:		Applications/Databases

%description ndb-tools
This package contains ndbcluster storage engine basic tools.

%{see_base}

%package ndb-extra
Summary:	MySQL - ndbcluster storage engine extra tools
Group:		Applications/Databases

%description ndb-extra
This package contains some extra ndbcluster storage engine tools for the advanced user.
They should be used with caution.

%{see_base}

%package bench
Requires: %{name}-client perl-DBI perl
Summary: MySQL - Benchmarks and test system
Group: Applications/Databases
Provides: mysql-bench
Obsoletes: mysql-bench

%description bench
This package contains MySQL benchmark scripts and data.

%{see_base}

%package devel
Summary: MySQL - Development header files and libraries
Group: Applications/Databases
Provides: mysql-devel
Obsoletes: mysql-devel

%description devel
This package contains the development header files and libraries
necessary to develop MySQL client applications.

%{see_base}

%package shared
Summary: MySQL - Shared libraries
Group: Applications/Databases

%description shared
This package contains the shared libraries (*.so*) which certain
languages and applications need to dynamically load and use MySQL.

%package Max
Summary: MySQL - server with extended functionality
Group: Applications/Databases
Provides: mysql-Max
Obsoletes: mysql-Max
Requires: MySQL-server >= @MYSQL_BASE_VERSION@

%description Max 
Optional MySQL server binary that supports additional features like:

 - Berkeley DB Storage Engine
 - Ndbcluster Storage Engine interface
 - Archive Storage Engine
 - CSV Storage Engine
 - Example Storage Engine
 - Federated Storage Engine
 - User Defined Functions (UDFs).

To activate this binary, just install this package in addition to
the standard MySQL package.

Please note that this is a dynamically linked binary!

%package embedded
Requires: %{name}-devel
Summary: MySQL - embedded library
Group: Applications/Databases
Obsoletes: mysql-embedded

%description embedded
This package contains the MySQL server as an embedded library.

The embedded MySQL server library makes it possible to run a
full-featured MySQL server inside the client application.
The main benefits are increased speed and more simple management
for embedded applications.

The API is identical for the embedded MySQL version and the
client/server version.

%{see_base}

%prep
%setup -n mysql-%{mysql_version}

%build

BuildMySQL() {
# The --enable-assembler simply does nothing on systems that does not
# support assembler speedups.
sh -c  "PATH=\"${MYSQL_BUILD_PATH:-$PATH}\" \
	CC=\"${CC:-$MYSQL_BUILD_CC}\" \
	CXX=\"${CXX:-$MYSQL_BUILD_CXX}\" \
	CFLAGS=\"${MYSQL_BUILD_CFLAGS:-$RPM_OPT_FLAGS}\" \
	CXXFLAGS=\"${MYSQL_BUILD_CXXFLAGS:-$RPM_OPT_FLAGS \
	          -felide-constructors -fno-exceptions -fno-rtti \
		  }\" \
	./configure \
 	    $* \
	    --enable-assembler \
	    --enable-local-infile \
            --with-mysqld-user=%{mysqld_user} \
            --with-unix-socket-path=/var/lib/mysql/mysql.sock \
            --prefix=/ \
	    --with-extra-charsets=complex \
            --exec-prefix=%{_exec_prefix} \
            --libexecdir=%{_sbindir} \
            --libdir=%{_libdir} \
            --sysconfdir=%{_sysconfdir} \
            --datadir=%{_datadir} \
            --localstatedir=%{mysqldatadir} \
            --infodir=%{_infodir} \
            --includedir=%{_includedir} \
            --mandir=%{_mandir} \
	    --enable-thread-safe-client \
	    --with-readline ;
	    # Add this for more debugging support
	    # --with-debug
	    # Add this for MyISAM RAID support:
	    # --with-raid
	    "

 # benchdir does not fit in above model. Maybe a separate bench distribution
 make benchdir_root=$RPM_BUILD_ROOT/usr/share/
}

# Use our own copy of glibc

OTHER_LIBC_DIR=/usr/local/mysql-glibc
USE_OTHER_LIBC_DIR=""
if test -d "$OTHER_LIBC_DIR"
then
  USE_OTHER_LIBC_DIR="--with-other-libc=$OTHER_LIBC_DIR"
fi

# Use the build root for temporary storage of the shared libraries.

RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}

# Clean up the BuildRoot first
[ "$RBR" != "/" ] && [ -d $RBR ] && rm -rf $RBR;
mkdir -p $RBR%{_libdir}/mysql

#
# Use MYSQL_BUILD_PATH so that we can use a dedicated version of gcc
#
PATH=${MYSQL_BUILD_PATH:-/bin:/usr/bin}
export PATH

# Build the Max binary (includes BDB and UDFs and therefore
# cannot be linked statically against the patched glibc)

# Use gcc for C and C++ code (to avoid a dependency on libstdc++ and
# including exceptions into the code
if [ -z "$CXX" -a -z "$CC" ]
then
	export CC="gcc"
	export CXX="gcc"
fi

BuildMySQL "--enable-shared \
		--without-openssl \
		--with-berkeley-db \
		--with-innodb \
		--with-ndbcluster \
		--with-archive \
		--with-csv-storage-engine \
		--with-example-storage-engine \
		--with-blackhole-storage-engine \
		--with-federated-storage-engine \
		--with-embedded-server \
		--with-comment=\"MySQL Community Edition - Max (GPL)\" \
		--with-server-suffix='-Max'"

make test

# Save mysqld-max
mv sql/mysqld sql/mysqld-max
nm --numeric-sort sql/mysqld-max > sql/mysqld-max.sym

# Install the ndb binaries
(cd ndb; make install DESTDIR=$RBR)

# Install embedded server library in the build root
install -m 644 libmysqld/libmysqld.a $RBR%{_libdir}/mysql/

# Include libgcc.a in the devel subpackage (BUG 4921)
if [ "$CC" = gcc ]
then
  libgcc=`$CC --print-libgcc-file`
  if [ -f $libgcc ]
  then
    %define have_libgcc 1
    install -m 644 $libgcc $RBR%{_libdir}/mysql/libmygcc.a
  fi
fi

# Save libraries
(cd libmysql/.libs; tar cf $RBR/shared-libs.tar *.so*)
(cd libmysql_r/.libs; tar rf $RBR/shared-libs.tar *.so*)
(cd ndb/src/.libs; tar rf $RBR/shared-libs.tar *.so*)

# Now clean up
make clean

#
# Only link statically on our i386 build host (which has a specially
# patched static glibc installed) - ia64 and x86_64 run glibc-2.3 (unpatched)
# so don't link statically there
#
BuildMySQL "--disable-shared \
%if %{STATIC_BUILD}
		--with-mysqld-ldflags='-all-static' \
		--with-client-ldflags='-all-static' \
		$USE_OTHER_LIBC_DIR \
%endif
		--with-comment=\"MySQL Community Edition - Standard (GPL)\" \
		--with-server-suffix='%{server_suffix}' \
		--without-embedded-server \
		--without-berkeley-db \
		--with-innodb \
		--without-vio \
		--without-openssl"
nm --numeric-sort sql/mysqld > sql/mysqld.sym

make test

%install
RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}

# Ensure that needed directories exists
install -d $RBR%{_sysconfdir}/{logrotate.d,init.d}
install -d $RBR%{mysqldatadir}/mysql
install -d $RBR%{_datadir}/{sql-bench,mysql-test}
install -d $RBR%{_includedir}
install -d $RBR%{_libdir}
install -d $RBR%{_mandir}
install -d $RBR%{_sbindir}


# Install all binaries stripped 
make install-strip DESTDIR=$RBR benchdir_root=%{_datadir}

# Install shared libraries (Disable for architectures that don't support it)
(cd $RBR%{_libdir}; tar xf $RBR/shared-libs.tar; rm -f $RBR/shared-libs.tar)

# install saved mysqld-max
install -s -m755 $MBD/sql/mysqld-max $RBR%{_sbindir}/mysqld-max

# install symbol files ( for stack trace resolution)
install -m644 $MBD/sql/mysqld-max.sym $RBR%{_libdir}/mysql/mysqld-max.sym
install -m644 $MBD/sql/mysqld.sym $RBR%{_libdir}/mysql/mysqld.sym

# Install logrotate and autostart
install -m644 $MBD/support-files/mysql-log-rotate $RBR%{_sysconfdir}/logrotate.d/mysql
install -m755 $MBD/support-files/mysql.server $RBR%{_sysconfdir}/init.d/mysql

# Create a symlink "rcmysql", pointing to the init.script. SuSE users
# will appreciate that, as all services usually offer this.
ln -s %{_sysconfdir}/init.d/mysql $RPM_BUILD_ROOT%{_sbindir}/rcmysql

# Create symbolic compatibility link safe_mysqld -> mysqld_safe
# (safe_mysqld will be gone in MySQL 4.1)
ln -sf ./mysqld_safe $RBR%{_bindir}/safe_mysqld

# Touch the place where the my.cnf config file and mysqlmanager.passwd
# (MySQL Instance Manager password file) might be located
# Just to make sure it's in the file list and marked as a config file
touch $RBR%{_sysconfdir}/my.cnf
touch $RBR%{_sysconfdir}/mysqlmanager.passwd

%pre server
# Shut down a previously installed server first
if test -x %{_sysconfdir}/init.d/mysql
then
  %{_sysconfdir}/init.d/mysql stop > /dev/null 2>&1
  echo "Giving mysqld a couple of seconds to exit nicely"
  sleep 5
elif test -x %{_sysconfdir}/rc.d/init.d/mysql
then
  %{_sysconfdir}/rc.d/init.d/mysql stop > /dev/null 2>&1
  echo "Giving mysqld a couple of seconds to exit nicely"
  sleep 5
fi

%post server
mysql_datadir=%{mysqldatadir}

# Create data directory if needed
if test ! -d $mysql_datadir; then mkdir -m755 $mysql_datadir; fi
if test ! -d $mysql_datadir/mysql; then mkdir $mysql_datadir/mysql; fi
if test ! -d $mysql_datadir/test; then mkdir $mysql_datadir/test; fi

# Make MySQL start/shutdown automatically when the machine does it.
# use insserv for older SuSE Linux versions
if test -x /sbin/insserv
then
	/sbin/insserv %{_sysconfdir}/init.d/mysql
# use chkconfig on Red Hat and newer SuSE releases
elif test -x /sbin/chkconfig
then
	/sbin/chkconfig --add mysql
fi

# Create a MySQL user. Do not report any problems if it already
# exists. This is redhat specific and should be handled more portable
useradd -M -r -d $mysql_datadir -s /bin/bash -c "MySQL server" %{mysqld_user} 2> /dev/null || true 

# Change permissions so that the user that will run the MySQL daemon
# owns all database files.
chown -R %{mysqld_user} $mysql_datadir

# Initiate databases
%{_bindir}/mysql_install_db --rpm --user=%{mysqld_user}

# Change permissions again to fix any new files.
chown -R %{mysqld_user} $mysql_datadir

# Fix permissions for the permission database so that only the user
# can read them.
chmod -R og-rw $mysql_datadir/mysql

# Restart in the same way that mysqld will be started normally.
%{_sysconfdir}/init.d/mysql start

# Allow safe_mysqld to start mysqld and print a message before we exit
sleep 2


%post ndb-storage
mysql_clusterdir=/var/lib/mysql-cluster

# Create cluster directory if needed
if test ! -d $mysql_clusterdir; then mkdir -m755 $mysql_clusterdir; fi


%post Max
# Restart mysqld, to use the new binary.
echo "Restarting mysqld."
%{_sysconfdir}/init.d/mysql restart > /dev/null 2>&1

%preun server
if test $1 = 0
then
	# Stop MySQL before uninstalling it
  if test -x %{_sysconfdir}/init.d/mysql
  then
    %{_sysconfdir}/init.d/mysql stop > /dev/null
  fi

  # Remove autostart of mysql
	# for older SuSE Linux versions
	if test -x /sbin/insserv
	then
		/sbin/insserv -r %{_sysconfdir}/init.d/mysql
	# use chkconfig on Red Hat and newer SuSE releases
	elif test -x /sbin/chkconfig
	then
		/sbin/chkconfig --del mysql
	fi
fi

# We do not remove the mysql user since it may still own a lot of
# database files.

# Clean up the BuildRoot
%clean
[ "$RBR" != "/" ] && [ -d $RBR ] && rm -rf $RBR;

%files server
%defattr(-,root,root,0755)

%doc COPYING README 
%doc support-files/my-*.cnf
%doc support-files/ndb-*.ini

%doc %attr(644, root, root) %{_infodir}/mysql.info*

%doc %attr(644, root, man) %{_mandir}/man1/mysql_zap.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysql_fix_privilege_tables.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld_multi.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld_safe.1*
%doc %attr(644, root, man) %{_mandir}/man1/perror.1*
%doc %attr(644, root, man) %{_mandir}/man1/replace.1*

%ghost %config(noreplace,missingok) %{_sysconfdir}/my.cnf
%ghost %config(noreplace,missingok) %{_sysconfdir}/mysqlmanager.passwd

%attr(755, root, root) %{_bindir}/my_print_defaults
%attr(755, root, root) %{_bindir}/myisamchk
%attr(755, root, root) %{_bindir}/myisam_ftdump
%attr(755, root, root) %{_bindir}/myisamlog
%attr(755, root, root) %{_bindir}/myisampack
%attr(755, root, root) %{_bindir}/mysql_convert_table_format
%attr(755, root, root) %{_bindir}/mysql_create_system_tables
%attr(755, root, root) %{_bindir}/mysql_explain_log
%attr(755, root, root) %{_bindir}/mysql_fix_extensions
%attr(755, root, root) %{_bindir}/mysql_fix_privilege_tables
%attr(755, root, root) %{_bindir}/mysql_install_db
%attr(755, root, root) %{_bindir}/mysql_secure_installation
%attr(755, root, root) %{_bindir}/mysql_setpermission
%attr(755, root, root) %{_bindir}/mysql_tzinfo_to_sql
%attr(755, root, root) %{_bindir}/mysql_zap
%attr(755, root, root) %{_bindir}/mysqlbug
%attr(755, root, root) %{_bindir}/mysqld_multi
%attr(755, root, root) %{_bindir}/mysqld_safe
%attr(755, root, root) %{_bindir}/mysqlhotcopy
%attr(755, root, root) %{_bindir}/mysqltest
%attr(755, root, root) %{_bindir}/perror
%attr(755, root, root) %{_bindir}/replace
%attr(755, root, root) %{_bindir}/resolve_stack_dump
%attr(755, root, root) %{_bindir}/resolveip
%attr(755, root, root) %{_bindir}/safe_mysqld

%attr(755, root, root) %{_sbindir}/mysqld
%attr(755, root, root) %{_sbindir}/mysqlmanager
%attr(755, root, root) %{_sbindir}/rcmysql
%attr(644, root, root) %{_libdir}/mysql/mysqld.sym

%attr(644, root, root) %config(noreplace,missingok) %{_sysconfdir}/logrotate.d/mysql
%attr(755, root, root) %{_sysconfdir}/init.d/mysql

%attr(755, root, root) %{_datadir}/mysql/

%files client
%defattr(-, root, root, 0755)
%attr(755, root, root) %{_bindir}/msql2mysql
%attr(755, root, root) %{_bindir}/mysql
%attr(755, root, root) %{_bindir}/mysql_find_rows
%attr(755, root, root) %{_bindir}/mysql_tableinfo
%attr(755, root, root) %{_bindir}/mysql_waitpid
%attr(755, root, root) %{_bindir}/mysqlaccess
%attr(755, root, root) %{_bindir}/mysqladmin
%attr(755, root, root) %{_bindir}/mysqlbinlog
%attr(755, root, root) %{_bindir}/mysqlcheck
%attr(755, root, root) %{_bindir}/mysqldump
%attr(755, root, root) %{_bindir}/mysqldumpslow
%attr(755, root, root) %{_bindir}/mysqlimport
%attr(755, root, root) %{_bindir}/mysqlshow

%doc %attr(644, root, man) %{_mandir}/man1/mysql.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqlaccess.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqladmin.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqldump.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqlshow.1*

%post shared
/sbin/ldconfig

%postun shared
/sbin/ldconfig

%files ndb-storage
%defattr(-,root,root,0755)
%attr(755, root, root) %{_sbindir}/ndbd

%files ndb-management
%defattr(-,root,root,0755)
%attr(755, root, root) %{_sbindir}/ndb_mgmd
%attr(755, root, root) %{_bindir}/ndb_mgm

%files ndb-tools
%defattr(-,root,root,0755)
%attr(755, root, root) %{_bindir}/ndb_mgm
%attr(755, root, root) %{_bindir}/ndb_restore
%attr(755, root, root) %{_bindir}/ndb_waiter
%attr(755, root, root) %{_bindir}/ndb_select_all
%attr(755, root, root) %{_bindir}/ndb_select_count
%attr(755, root, root) %{_bindir}/ndb_desc
%attr(755, root, root) %{_bindir}/ndb_show_tables
%attr(755, root, root) %{_bindir}/ndb_test_platform

%files ndb-extra
%defattr(-,root,root,0755)
%attr(755, root, root) %{_bindir}/ndb_drop_index
%attr(755, root, root) %{_bindir}/ndb_drop_table
%attr(755, root, root) %{_bindir}/ndb_delete_all

%files devel
%defattr(-, root, root, 0755)
%doc EXCEPTIONS-CLIENT
%attr(755, root, root) %{_bindir}/comp_err
%attr(755, root, root) %{_bindir}/mysql_config
%dir %attr(755, root, root) %{_includedir}/mysql
%dir %attr(755, root, root) %{_libdir}/mysql
%{_includedir}/mysql/*
%{_libdir}/mysql/libdbug.a
%{_libdir}/mysql/libheap.a
%if %{have_libgcc}
%{_libdir}/mysql/libmygcc.a
%endif
%{_libdir}/mysql/libmyisam.a
%{_libdir}/mysql/libmyisammrg.a
%{_libdir}/mysql/libmysqlclient.a
%{_libdir}/mysql/libmysqlclient.la
%{_libdir}/mysql/libmysqlclient_r.a
%{_libdir}/mysql/libmysqlclient_r.la
%{_libdir}/mysql/libmystrings.a
%{_libdir}/mysql/libmysys.a
%{_libdir}/mysql/libndbclient.a
%{_libdir}/mysql/libndbclient.la
%{_libdir}/mysql/libvio.a

%files shared
%defattr(-, root, root, 0755)
# Shared libraries (omit for architectures that don't support them)
%{_libdir}/*.so*

%files bench
%defattr(-, root, root, 0755)
%attr(-, root, root) %{_datadir}/sql-bench
%attr(-, root, root) %{_datadir}/mysql-test
%attr(755, rott, root) %{_bindir}/mysql_client_test
%attr(755, root, root) %{_bindir}/mysqltestmanager
%attr(755, root, root) %{_bindir}/mysqltestmanager-pwgen
%attr(755, root, root) %{_bindir}/mysqltestmanagerc

%files Max
%defattr(-, root, root, 0755)
%attr(755, root, root) %{_sbindir}/mysqld-max
%attr(644, root, root) %{_libdir}/mysql/mysqld-max.sym

%files embedded
%defattr(-, root, root, 0755)
%attr(644, root, root) %{_libdir}/mysql/libmysqld.a

# The spec file changelog only includes changes made to the spec file
# itself - note that they must be ordered by date (important when
# merging BK trees)
%changelog 
* Tue Jun 14 2005 Lenz Grimmer <lenz@mysql.com>

- Do not build statically on i386 by default, only when adding either "--with
  static" or "--define '_with_static 1'" to the RPM build options. Static
  linking really only makes sense when linking against the specially patched
  glibc 2.2.5.

* Mon Jun 06 2005 Lenz Grimmer <lenz@mysql.com>

- added mysql_client_test to the "bench" subpackage (BUG 10676)
- added the libndbclient static and shared libraries (BUG 10676)

* Wed Jun 01 2005 Lenz Grimmer <lenz@mysql.com>

- use "mysqldatadir" variable instead of hard-coding the path multiple times
- use the "mysqld_user" variable on all occasions a user name is referenced
- removed (incomplete) Brazilian translations
- removed redundant release tags from the subpackage descriptions

* Wed May 25 2005 Joerg Bruehe <joerg@mysql.com>

- Added a "make clean" between separate calls to "BuildMySQL".

* Wed Apr 20 2005 Lenz Grimmer <lenz@mysql.com>

- Enabled the "blackhole" storage engine for the Max RPM

* Wed Apr 13 2005 Lenz Grimmer <lenz@mysql.com>

- removed the MySQL manual files (html/ps/texi) - they have been removed
  from the MySQL sources and are now available seperately.

* Mon Apr 4 2005 Petr Chardin <petr@mysql.com>

- old mysqlmanager, mysqlmanagerc and mysqlmanager-pwger renamed into
  mysqltestmanager, mysqltestmanager and mysqltestmanager-pwgen respectively

* Fri Mar 18 2005 Lenz Grimmer <lenz@mysql.com>

- Disabled RAID in the Max binaries once and for all (it has finally been
  removed from the source tree)

* Sun Feb 20 2005 Petr Chardin <petr@mysql.com>

- Install MySQL Instance Manager together with mysqld, touch mysqlmanager
  password file

* Mon Feb 14 2005 Lenz Grimmer <lenz@mysql.com>

- Fixed the compilation comments and moved them into the separate build sections
  for Max and Standard

* Mon Feb 7 2005 Tomas Ulin <tomas@mysql.com>

- enabled the "Ndbcluster" storage engine for the max binary
- added extra make install in ndb subdir after Max build to get ndb binaries
- added packages for ndbcluster storage engine

* Fri Jan 14 2005 Lenz Grimmer <lenz@mysql.com>

- replaced obsoleted "BuildPrereq" with "BuildRequires" instead

* Thu Jan 13 2005 Lenz Grimmer <lenz@mysql.com>

- enabled the "Federated" storage engine for the max binary

* Tue Jan 04 2005 Petr Chardin <petr@mysql.com>

- ISAM and merge storage engines were purged. As well as appropriate
  tools and manpages (isamchk and isamlog)

* Thu Dec 31 2004 Lenz Grimmer <lenz@mysql.com>

- enabled the "Archive" storage engine for the max binary
- enabled the "CSV" storage engine for the max binary
- enabled the "Example" storage engine for the max binary

* Thu Aug 26 2004 Lenz Grimmer <lenz@mysql.com>

- MySQL-Max now requires MySQL-server instead of MySQL (BUG 3860)

* Fri Aug 20 2004 Lenz Grimmer <lenz@mysql.com>

- do not link statically on IA64/AMD64 as these systems do not have
  a patched glibc installed

* Tue Aug 10 2004 Lenz Grimmer <lenz@mysql.com>

- Added libmygcc.a to the devel subpackage (required to link applications
  against the the embedded server libmysqld.a) (BUG 4921)

* Mon Aug 09 2004 Lenz Grimmer <lenz@mysql.com>

- Added EXCEPTIONS-CLIENT to the "devel" package

* Thu Jul 29 2004 Lenz Grimmer <lenz@mysql.com>

- disabled OpenSSL in the Max binaries again (the RPM packages were the
  only exception to this anyway) (BUG 1043)

* Wed Jun 30 2004 Lenz Grimmer <lenz@mysql.com>

- fixed server postinstall (mysql_install_db was called with the wrong
  parameter)

* Thu Jun 24 2004 Lenz Grimmer <lenz@mysql.com>

- added mysql_tzinfo_to_sql to the server subpackage
- run "make clean" instead of "make distclean"

* Mon Apr 05 2004 Lenz Grimmer <lenz@mysql.com>

- added ncurses-devel to the build prerequisites (BUG 3377)

* Thu Feb 12 2004 Lenz Grimmer <lenz@mysql.com>

- when using gcc, _always_ use CXX=gcc 
- replaced Copyright with License field (Copyright is obsolete)

* Tue Feb 03 2004 Lenz Grimmer <lenz@mysql.com>

- added myisam_ftdump to the Server package

* Tue Jan 13 2004 Lenz Grimmer <lenz@mysql.com>

- link the mysql client against libreadline instead of libedit (BUG 2289)

* Mon Dec 22 2003 Lenz Grimmer <lenz@mysql.com>

- marked /etc/logrotate.d/mysql as a config file (BUG 2156)

* Fri Dec 13 2003 Lenz Grimmer <lenz@mysql.com>

- fixed file permissions (BUG 1672)

* Thu Dec 11 2003 Lenz Grimmer <lenz@mysql.com>

- made testing for gcc3 a bit more robust

* Fri Dec 05 2003 Lenz Grimmer <lenz@mysql.com>

- added missing file mysql_create_system_tables to the server subpackage

* Fri Nov 21 2003 Lenz Grimmer <lenz@mysql.com>

- removed dependency on MySQL-client from the MySQL-devel subpackage
  as it is not really required. (BUG 1610)

* Fri Aug 29 2003 Lenz Grimmer <lenz@mysql.com>

- Fixed BUG 1162 (removed macro names from the changelog)
- Really fixed BUG 998 (disable the checking for installed but
  unpackaged files)

* Tue Aug 05 2003 Lenz Grimmer <lenz@mysql.com>

- Fixed BUG 959 (libmysqld not being compiled properly)
- Fixed BUG 998 (RPM build errors): added missing files to the
  distribution (mysql_fix_extensions, mysql_tableinfo, mysqldumpslow,
  mysql_fix_privilege_tables.1), removed "-n" from install section.

* Wed Jul 09 2003 Lenz Grimmer <lenz@mysql.com>

- removed the GIF Icon (file was not included in the sources anyway)
- removed unused variable shared_lib_version
- do not run automake before building the standard binary
  (should not be necessary)
- add server suffix '-standard' to standard binary (to be in line
  with the binary tarball distributions)
- Use more RPM macros (_exec_prefix, _sbindir, _libdir, _sysconfdir,
  _datadir, _includedir) throughout the spec file.
- allow overriding CC and CXX (required when building with other compilers)

* Fri May 16 2003 Lenz Grimmer <lenz@mysql.com>

- re-enabled RAID again

* Wed Apr 30 2003 Lenz Grimmer <lenz@mysql.com>

- disabled MyISAM RAID (--with-raid) - it throws an assertion which
  needs to be investigated first.

* Mon Mar 10 2003 Lenz Grimmer <lenz@mysql.com>

- added missing file mysql_secure_installation to server subpackage
  (BUG 141)

* Tue Feb 11 2003 Lenz Grimmer <lenz@mysql.com>

- re-added missing pre- and post(un)install scripts to server subpackage
- added config file /etc/my.cnf to the file list (just for completeness)
- make sure to create the datadir with 755 permissions

* Mon Jan 27 2003 Lenz Grimmer <lenz@mysql.com>

- removed unused CC and CXX variables
- CFLAGS and CXXFLAGS should honor RPM_OPT_FLAGS

* Fri Jan 24 2003 Lenz Grimmer <lenz@mysql.com>

- renamed package "MySQL" to "MySQL-server"
- fixed Copyright tag
- added mysql_waitpid to client subpackage (required for mysql-test-run)

* Wed Nov 27 2002 Lenz Grimmer <lenz@mysql.com>

- moved init script from /etc/rc.d/init.d to /etc/init.d (the majority of 
  Linux distributions now support this scheme as proposed by the LSB either
  directly or via a compatibility symlink)
- Use new "restart" init script action instead of starting and stopping
  separately
- Be more flexible in activating the automatic bootup - use insserv (on
  older SuSE versions) or chkconfig (Red Hat, newer SuSE versions and
  others) to create the respective symlinks

* Wed Sep 25 2002 Lenz Grimmer <lenz@mysql.com>

- MySQL-Max now requires MySQL >= 4.0 to avoid version mismatches
  (mixing 3.23 and 4.0 packages)

* Fri Aug 09 2002 Lenz Grimmer <lenz@mysql.com>
 
- Turn off OpenSSL in MySQL-Max for now until it works properly again
- enable RAID for the Max binary instead
- added compatibility link: safe_mysqld -> mysqld_safe to ease the
  transition from 3.23

* Thu Jul 18 2002 Lenz Grimmer <lenz@mysql.com>

- Reworked the build steps a little bit: the Max binary is supposed
  to include OpenSSL, which cannot be linked statically, thus trying
	to statically link against a special glibc is futile anyway
- because of this, it is not required to make yet another build run
  just to compile the shared libs (saves a lot of time)
- updated package description of the Max subpackage
- clean up the BuildRoot directory afterwards

* Mon Jul 15 2002 Lenz Grimmer <lenz@mysql.com>

- Updated Packager information
- Fixed the build options: the regular package is supposed to
  include InnoDB and linked statically, while the Max package
	should include BDB and SSL support

* Fri May 03 2002 Lenz Grimmer <lenz@mysql.com>

- Use more RPM macros (e.g. infodir, mandir) to make the spec
  file more portable
- reorganized the installation of documentation files: let RPM
  take care of this
- reorganized the file list: actually install man pages along
  with the binaries of the respective subpackage
- do not include libmysqld.a in the devel subpackage as well, if we
  have a special "embedded" subpackage
- reworked the package descriptions

* Mon Oct  8 2001 Monty

- Added embedded server as a separate RPM

* Fri Apr 13 2001 Monty

- Added mysqld-max to the distribution

* Tue Jan 2  2001  Monty

- Added mysql-test to the bench package

* Fri Aug 18 2000 Tim Smith <tim@mysql.com>

- Added separate libmysql_r directory; now both a threaded
  and non-threaded library is shipped.

* Wed Sep 28 1999 David Axmark <davida@mysql.com>

- Added the support-files/my-example.cnf to the docs directory.

- Removed devel dependency on base since it is about client
  development.

* Wed Sep 8 1999 David Axmark <davida@mysql.com>

- Cleaned up some for 3.23.

* Thu Jul 1 1999 David Axmark <davida@mysql.com>

- Added support for shared libraries in a separate sub
  package. Original fix by David Fox (dsfox@cogsci.ucsd.edu)

- The --enable-assembler switch is now automatically disables on
  platforms there assembler code is unavailable. This should allow
  building this RPM on non i386 systems.

* Mon Feb 22 1999 David Axmark <david@detron.se>

- Removed unportable cc switches from the spec file. The defaults can
  now be overridden with environment variables. This feature is used
  to compile the official RPM with optimal (but compiler version
  specific) switches.

- Removed the repetitive description parts for the sub rpms. Maybe add
  again if RPM gets a multiline macro capability.

- Added support for a pt_BR translation. Translation contributed by
  Jorge Godoy <jorge@bestway.com.br>.

* Wed Nov 4 1998 David Axmark <david@detron.se>

- A lot of changes in all the rpm and install scripts. This may even
  be a working RPM :-)

* Sun Aug 16 1998 David Axmark <david@detron.se>

- A developers changelog for MySQL is available in the source RPM. And
  there is a history of major user visible changed in the Reference
  Manual.  Only RPM specific changes will be documented here.
