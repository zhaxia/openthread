#
#    Copyright (c) 2015 Nest Labs, Inc.
#    All rights reserved.
#
#    This document is the property of Nest. It is considered
#    confidential and proprietary information.
#
#    This document may not be reproduced or transmitted in any form,
#    in whole or in part, without the express written permission of
#    Nest.
#
#    Description:
#      This file defines GNU autoconf M4-style macros that ensure the
#      -Werror compiler option for GCC-based or -compatible compilers
#      do not break some autoconf tests (see
#      http://lists.gnu.org/archive/html/autoconf-patches/2008-09/msg00014.html).
#
#      If -Werror has been passed transform it into -Wno-error for
#      CPPFLAGS, CFLAGS, CXXFLAGS, OBJCFLAGS, and OBJCXXFLAGS with
#      NL_SAVE_WERROR. Transform them back again with
#      NL_RESTORE_WERROR.
#

# 
# _NL_SAVE_WERROR_FOR_VAR(variable)
#
#   variable - The compiler flags variable to scan for the presence of
#              -Werror and, if present, transform to -Wno-error.
#
# This transforms, for the specified compiler flags variable, -Werror
# to -Wno-error, if it was it present. The original state may be
# restored by invoking _NL_RESTORE_WERROR_FOR_VAR([variable]).
#
#------------------------------------------------------------------------------
AC_DEFUN([_NL_SAVE_WERROR_FOR_VAR],
[
    if echo "${$1}" | grep -q '\-Werror'; then
	$1="`echo ${$1} | sed -e 's,-Werror\([[[:space:]]]\),-Wno-error\1,g'`"
	nl_had_$1_werror=yes
    else
	nl_had_$1_werror=no
    fi
])

#
# _NL_RESTORE_WERROR_FOR_VAR(variable)
#
#   variable - The compiler flag for which to restore -Wno-error back
#              to -Werror if it was originally passed in by the user as
#              such.
#
# This restores, for the specified compiler flags variable, -Werror
# from -Wno-error, if it was initially set as -Werror at the time
# _NL_SAVE_WERROR_FOR_VAR([variable]) was invoked.
#
#------------------------------------------------------------------------------
AC_DEFUN([_NL_RESTORE_WERROR_FOR_VAR],
[
    if test "${nl_had_$1_werror}" = "yes"; then
	$1="`echo ${$1} | sed -e 's,-Wno-error\([[[:space:]]]\),-Werror\1,g'`"
    fi

    unset nl_had_$1_werror
])

# 
# NL_SAVE_WERROR
#
# This transforms, for each of CFLAGS, CXXFLAGS, OBJCFLAGS, and
# OBJCXXFLAGS, -Werror to -Wno-error, if it was it present. The
# original state may be restored by invoking NL_RESTORE_WERROR.
#
#------------------------------------------------------------------------------
AC_DEFUN([NL_SAVE_WERROR],
[
    _NL_SAVE_WERROR_FOR_VAR([CPPFLAGS])
    _NL_SAVE_WERROR_FOR_VAR([CFLAGS])
    _NL_SAVE_WERROR_FOR_VAR([CXXFLAGS])
    _NL_SAVE_WERROR_FOR_VAR([OBJCFLAGS])
    _NL_SAVE_WERROR_FOR_VAR([OBJCXXFLAGS])
])

#
# NL_RESTORE_WERROR
#
# This restores, for each of OBJCXXFLAGS, OBJCFLAGS, CXXFLAGS, and
# CFLAGS, -Werror from -Wno-error, if it was initially set as -Werror
# at the time NL_SAVE_WERROR was invoked.
#
#------------------------------------------------------------------------------
AC_DEFUN([NL_RESTORE_WERROR],
[
    _NL_RESTORE_WERROR_FOR_VAR([OBJCXXFLAGS])
    _NL_RESTORE_WERROR_FOR_VAR([OBJCFLAGS])
    _NL_RESTORE_WERROR_FOR_VAR([CXXFLAGS])
    _NL_RESTORE_WERROR_FOR_VAR([CFLAGS])
    _NL_RESTORE_WERROR_FOR_VAR([CPPFLAGS])
])
