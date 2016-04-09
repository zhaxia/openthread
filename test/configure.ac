#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 lib/Makefile
                 script/Makefile
                 src/Makefile
                 test/ncp/Makefile
                 test/soc/Makefile
                 test/thread-ctl/Makefile
                 test/thread-driver/Makefile])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

AM_CONDITIONAL(DARWIN, test `uname` = Darwin)
AM_CONDITIONAL(LINUX, test `uname` = Linux)

CXXFLAGS="-Wall -std=c++11 -DAUTOMAKE -Werror"

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([endian.h])
AC_CHECK_HEADERS([machine/endian.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_OUTPUT
