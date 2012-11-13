#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(mod_identicon, 0.0.1,[])

AC_CONFIG_SRCDIR([mod_identicon.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE(mod_identicon, 0.0.1, [NO-DEFINE])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_LIBTOOL

# Option for identicon debug log level
AC_ARG_WITH(identicon-debug-log,
  AC_HELP_STRING([--with-identicon-debug-log=LEVEL],
    [identicon debug log level [default=no]]),
  [IDENTICON_DEBUG_LOG="${withval}"],
  [IDENTICON_DEBUG_LOG=no]
)
AS_IF([test "x${IDENTICON_DEBUG_LOG}" != xno],
    [AC_DEFINE_UNQUOTED([AP_IDENTICON_DEBUG_LOG_LEVEL],
      [${IDENTICON_DEBUG_LOG}], [identicon debug log level])]
)

# Option for memcache support
AC_ARG_ENABLE(identicon-memcache,
  AC_HELP_STRING([--enable-identicon-memcache],
    [enable identicon memcache [default=no]]),
  [ENABLED_IDENTICON_MEMCACHE="${enableval:-yes}"],
  [ENABLED_IDENTICON_MEMCACHE=no]
)
AS_IF([test "x${ENABLED_IDENTICON_MEMCACHE}" = xyes],
    [AC_DEFINE([IDENTICON_HAVE_MEMCACHE], [1], [enable memcache])]
)

# Checks for apxs.
AC_ARG_WITH(apxs,
  [AC_HELP_STRING([--with-apxs=PATH], [apxs path [default=yes]])],
  [APXS_PATH="$withval"],
  [APXS_PATH="/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin"]
)

AC_PATH_PROG(APXS, apxs, no, ${APXS_PATH})

AS_IF([test "x${APXS}" = "xno"], AC_MSG_ERROR([apxs not found]),
  [test "x${APXS}" = "x"], AC_MSG_ERROR([apxs not found])
)

APXS_CFLAGS=`${APXS} -q CFLAGS 2> /dev/null`
APXS_CPPFLAGS=`${APXS} -q CPPFLAGS 2> /dev/null`
APXS_LDFLAGS=`${APXS} -q LDFLAGS 2> /dev/null`
APXS_LIBS=`${APXS} -q LIBS 2> /dev/null`
APXS_LIBEXECDIR=`${APXS} -q LIBEXECDIR 2> /dev/null`

APXS_INCLUDEDIR=`${APXS} -q INCLUDEDIR 2> /dev/null`
APXS_INCLUDES="-I${APXS_INCLUDEDIR}"

# Checks for apr.
AC_ARG_WITH(apr,
  [AC_HELP_STRING([--with-apr=PATH], [apr config path [default=yes]])],
  [APR_CONFIG="$withval"],
  [with_apr=yes]
)

AC_MSG_CHECKING([whether apr])
AS_IF([test "x$with_apr" != "xno"],
  [
   if test ! -x "${APR_CONFIG}" -o -z "${APR_CONFIG}"; then
     APR_PATH=`${APXS} -q APR_BINDIR 2> /dev/null`
     APR_CONFIG="${APR_PATH}/apr-1-config"
     if test ! -x "${APR_CONFIG}"; then
       APR_CONFIG="${APR_PATH}/apr-config"
     fi
   fi
   APR_INCLUDES=`${APR_CONFIG} --includes 2> /dev/null`
   APR_CFLAGS=`${APR_CONFIG} --cflags 2> /dev/null`
   APR_CPPFLAGS=`${APR_CONFIG} --cppflags 2> /dev/null`
   APR_LDFLAGS=`${APR_CONFIG} --ldflags 2> /dev/null`
   APR_LIBS=`${APR_CONFIG} --libs 2> /dev/null`
   AC_MSG_RESULT(yes)
  ],
  AC_MSG_ERROR(apr not found)
)

# Checks for apreq2.
AC_ARG_WITH(apreq2,
  [AC_HELP_STRING([--with-apreq2=PATH], [apreq2 config path [default=yes]])],
  [APREQ2_CONFIG="$withval"],
  [with_apreq2=yes]
)

AC_MSG_CHECKING([whether apreq2])
AS_IF([test "x$with_apreq2" != "xno"],
  [
   if test ! -x "${APREQ2_CONFIG}" -o -z "${APREQ2_CONFIG}"; then
     SEARCH_PATH="/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin"
     AC_PATH_PROG(APREQ2_CONFIG, apreq2-config, no, ${SEARCH_PATH})
     AS_IF(
       [test "x${APREQ2_CONFIG}" = "xno"], AC_MSG_ERROR([apreq2-config not found]),
       [test "x${APREQ2_CONFIG}" = "x"], AC_MSG_ERROR([apreq2-config not found])
     )
   fi
   APREQ2_INCLUDES=`${APREQ2_CONFIG} --includes 2> /dev/null`
   APREQ2_LDFLAGS=`${APREQ2_CONFIG} --ldflags 2> /dev/null`
   APREQ2_LIBS=`${APREQ2_CONFIG} --libs 2> /dev/null`
   AC_MSG_RESULT(yes)
  ],
  AC_MSG_ERROR(apreq2 not found)
)

# Apache libraries.
APACHE_MODULEDIR="${APXS_LIBEXECDIR}"
APACHE_INCLUDES="${APXS_INCLUDES} ${APR_INCLUDES} ${APREQ2_INCLUDES}"
APACHE_CFLAGS="${APXS_CFLAGS} ${APR_CFLAGS}"
APACHE_CPPFLAGS="${APXS_CPPFLAGS} ${APR_CPPFLAGS}"
APACHE_LDFLAGS="${APXS_LDFLAGS} ${APR_LDFLAGS} ${APREQ2_LDFLAGS}"
APACHE_LIBS="${APXS_LIBS} ${APR_LIBS} ${APREQ2_LIBS}"

AC_SUBST(APACHE_MODULEDIR)
AC_SUBST(APACHE_INCLUDES)
AC_SUBST(APACHE_CFLAGS)
AC_SUBST(APACHE_CPPFLAGS)
AC_SUBST(APACHE_LDFLAGS)
AC_SUBST(APACHE_LIBS)


# Checks for gd.
AC_ARG_WITH(gd,
  [AC_HELP_STRING([--with-gd=PATH], [gd path [default=/usr/include]])],
  [GD_PATH="$withval"],
  [GD_PATH="/usr/include"]
)

AC_CHECK_FILE(
  "${GD_PATH}/gd.h",
  [GD_INCLUDEDIR=${GD_PATH}],
  [AC_MSG_ERROR([Missing required gd header.])]
)

AC_ARG_WITH(gd-lib,
  [AC_HELP_STRING([--with-gd-lib=PATH], [gd library path [default=no]])],
  [GD_LIBPATH="$withval"],
  [GD_LIBPATH="no"]
)

AS_IF([test "x${GD_LIBPATH}" != "xno"],
  GD_LDFLAGS="-L${GD_LIBPATH}"
)

AC_MSG_CHECKING([for gd library])
SAVED_CFLAGS=$CFLAGS
SAVED_LDFLAGS=$LDFLAGS
CFLAGS="$CFLAGS -I ${GD_INCLUDEDIR}"
LDFLAGS="$LDFLAGS ${GD_LDFLAGS} -lgd"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
    [#include "gd.h"],
    [gdImagePtr img])],
  [GD_LIBS="-lgd"],
  [AC_MSG_ERROR([Missing required gd library.])]
)
AC_MSG_RESULT(yes)
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

# gd libraries.
GD_CFLAGS="-I ${GD_INCLUDEDIR}"
GD_CPPFLAGS="${GD_CPPFLAGS}"
GD_LDFLAGS="${GD_LDFLAGS} ${GD_LIBS}"

AC_SUBST(GD_CFLAGS)
AC_SUBST(GD_CPPFLAGS)
AC_SUBST(GD_LDFLAGS)
AC_SUBST(GD_LIBS)


# Checks for libmemcached.
AS_IF([test "x${ENABLED_IDENTICON_MEMCACHE}" = xyes],[
AC_ARG_WITH(libmemcached,
  [AC_HELP_STRING([--with-libmemcached=PATH], [libmemcached path [default=/usr/include/libmemcached]])],
  [LIBMEMCACHED_PATH="$withval"],
  [LIBMEMCACHED_PATH="/usr/include/libmemcached"]
)

AC_CHECK_FILE(
  "${LIBMEMCACHED_PATH}/memcached.h",
  [LIBMEMCACHED_INCLUDEDIR=${LIBMEMCACHED_PATH}],
  [AC_MSG_ERROR([Missing required libmemcached header.])]
)

AC_ARG_WITH(libmemcached-lib,
  [AC_HELP_STRING([--with-libmemcached-lib=PATH], [libmemcached library path [default=no]])],
  [LIBMEMCACHED_LIBPATH="$withval"],
  [LIBMEMCACHED_LIBPATH="no"]
)

AS_IF([test "x${LIBMEMCACHED_LIBPATH}" != "xno"],
  LIBMEMCACHED_LDFLAGS="-L${LIBMEMCACHED_LIBPATH}"
)

AC_MSG_CHECKING([for libmemcached library])
SAVED_CFLAGS=$CFLAGS
SAVED_LDFLAGS=$LDFLAGS
CFLAGS="$CFLAGS -I ${LIBMEMCACHED_INCLUDEDIR}"
LDFLAGS="$LDFLAGS ${LIBMEMCACHED_LDFLAGS} -lmemcached"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
    [#include "memcached.h"],
    [struct memcached_st *memc])],
  [LIBMEMCACHED_LIBS="-lmemcached"],
  [AC_MSG_ERROR([Missing required libmemcached library.])]
)
AC_MSG_RESULT(yes)
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

# libmemcached libraries.
LIBMEMCACHED_CFLAGS="-I ${LIBMEMCACHED_INCLUDEDIR}"
LIBMEMCACHED_CPPFLAGS="${LIBMEMCACHED_CPPFLAGS}"
LIBMEMCACHED_LDFLAGS="${LIBMEMCACHED_LDFLAGS} ${LIBMEMCACHED_LIBS}"
])

AC_SUBST(LIBMEMCACHED_CFLAGS)
AC_SUBST(LIBMEMCACHED_CPPFLAGS)
AC_SUBST(LIBMEMCACHED_LDFLAGS)
AC_SUBST(LIBMEMCACHED_LIBS)


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
