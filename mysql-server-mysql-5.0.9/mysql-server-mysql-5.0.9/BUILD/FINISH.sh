cflags="$c_warnings $extra_flags"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags"
extra_configs="$extra_configs $local_infile_configs"
configure="./configure $base_configs $extra_configs"

commands="\
$make -k distclean || true 
/bin/rm -rf */.deps/*.P config.cache innobase/config.cache bdb/build_unix/config.cache bdb/dist/autom4te.cache autom4te.cache innobase/autom4te.cache;


path=`dirname $0`
. \"$path/autorun.sh\""

if [ -z "$just_clean" ]
then
commands="$commands
CFLAGS=\"$cflags\" CXX=\"$CXX\" CXXFLAGS=\"$cxxflags\" CXXLDFLAGS=\"$CXXLDFLAGS\" \
$configure"
fi

if [ -z "$just_configure" -a -z "$just_clean" ]
then
  commands="$commands

$make $AM_MAKEFLAGS"

  if [ "x$strip" = "xyes" ]
  then
    commands="$commands

mkdir -p tmp
nm --numeric-sort sql/mysqld  > tmp/mysqld.sym
objdump -d sql/mysqld > tmp/mysqld.S
strip sql/mysqld"
  fi
fi

if test -z "$just_print"
then
  eval "set -x; $commands"
else
  echo "$commands"
fi
