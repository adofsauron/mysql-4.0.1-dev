AC_CONFIG_FILES(extra/yassl/Makefile dnl
extra/yassl/taocrypt/Makefile dnl
extra/yassl/taocrypt/src/Makefile dnl
extra/yassl/src/Makefile)

AC_DEFUN([MYSQL_CHECK_YASSL], [
  AC_MSG_CHECKING(for yaSSL)
  AC_ARG_WITH([yassl],
              [  --with-yassl          Include the yaSSL support],
              [yassl=yes],
              [yassl=no])

  if test "$yassl" = "yes"
  then
    if test "$openssl" != "no"
    then
      AC_MSG_ERROR([Cannot configure MySQL to use yaSSL and OpenSSL simultaneously.])
    fi
    AC_MSG_RESULT([using bundled yaSSL])
    yassl_dir="extra/yassl"
    openssl_libs="\
    -L\$(top_builddir)/extra/yassl/src -lyassl\
    -L\$(top_builddir)/extra/yassl/taocrypt/src -ltaocrypt"
    openssl_includes="-I\$(top_srcdir)/extra/yassl/include"
    AC_DEFINE([HAVE_OPENSSL], [1], [Defined by configure. Using yaSSL for OpenSSL emulation.])
  else
    yassl_dir=""
    AC_MSG_RESULT(no)
  fi
  AC_SUBST(openssl_libs)
  AC_SUBST(openssl_includes)
  AC_SUBST(yassl_dir)
  AM_CONDITIONAL([HAVE_YASSL], [ test "$yassl" = "yes" ])
])
