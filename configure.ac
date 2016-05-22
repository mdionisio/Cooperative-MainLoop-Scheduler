#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([schedev], [0.0], [michele.dionisio@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/bin/task.c])
AC_CONFIG_HEADERS([config.h])


AM_INIT_AUTOMAKE

AC_USE_SYSTEM_EXTENSIONS

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

# Checks for programs.
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_PROG_LIBTOOL

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h stddef.h stdint.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([getpagesize gettimeofday memmove memset strdup strerror])

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [enable debugging, default: no]),
[case "${enableval}" in
             yes) debug=true ;;
             no)  debug=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=false])

if test "x$debug" = "xtrue"; then
 AC_DEFINE([HAVE_DEBUG], [1], [Enable DEBUG.])
fi

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

# ##################################

AC_ARG_ENABLE(profile,
AS_HELP_STRING([--enable-profile],
               [enable profiling, default: no]),
[case "${enableval}" in
             yes) profile=true ;;
             no)  dprofile=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-profile]) ;;
esac],
[profile=false])

if test "x$profile" = "xtrue"; then
 AC_DEFINE([HAVE_PROFILE], [1], [Enable PROFILE.])
fi

AM_CONDITIONAL(PROFILE, test x"$profile" = x"true")

# ##################################

PKG_CHECK_MODULES([LIBEVENT], [libevent])

# ##################################

PKG_CHECK_MODULES([LIBUV], [libuv])

# ##################################

# ##################################

AC_ARG_ENABLE(pthread,
AS_HELP_STRING([--enable-pthread],
               [enable pthread, default: no]),
[case "${enableval}" in
             yes) pthread=true ;;
             no)  pthread=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-pthread]) ;;
esac],
[pthread=false])

if test "x$pthread" = "xtrue"; then
 AC_DEFINE([HAVE_PTHREAD], [1], [Enable Pthread.])
 PKG_CHECK_MODULES([LIBEVENTPTHREAD], [libevent_pthreads])
fi

AM_CONDITIONAL(PTHREAD, test x"$pthread" = x"true")

# ##################################

AC_CONFIG_FILES([Makefile
                 src/bin/Makefile])

AC_OUTPUT
