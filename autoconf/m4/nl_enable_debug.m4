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
#      This file defines a GNU autoconf M4-style macro that adds an
#      --enable-debug configuration option to the package and controls
#      whether the package will be built for debug instances of programs
#      and libraries.
#

#
# NL_ENABLE_DEBUG(default)
#
#   default - Whether the option should be enabled (yes) or disabled (no)
#             by default.
#
# Adds an --enable-debug configuration option to the package with a
# default value of 'default' (should be either 'no' or 'yes') and controls
# whether the package will be built with or without -DDEBUG enabled.
#
# The value 'nl_cv_build_debug' will be set to the result. In
# addition, the contents of CFLAGS, CXXFLAGS, OBJCFLAGS, and
# OBJCXXFLAGS may be altered by the use of this macro, adding -DDEBUG
# if this option is asserted.
#
#------------------------------------------------------------------------------
AC_DEFUN([NL_ENABLE_DEBUG],
[
    # Check whether or not a default value has been passed in.

    m4_case([$1],
        [yes],[],
        [no],[],
        [m4_fatal([$0: invalid default value '$1'; must be 'yes' or 'no'])])

    AC_CACHE_CHECK([whether to build debug instances of programs and libraries],
        nl_cv_build_debug,
        [
            AC_ARG_ENABLE(debug,
                [AS_HELP_STRING([--enable-debug],[Enable the generation of debug instances @<:@default=$1@:>@.])],
                [
                    case "${enableval}" in 

                    no|yes)
                        nl_cv_build_debug=${enableval}
                        ;;

                    *)
                        AC_MSG_ERROR([Invalid value ${enableval} for --enable-debug])
                        ;;

                    esac
                ],
                [
                    nl_cv_build_debug=$1
                ])

            if test "${nl_cv_build_debug}" = "yes"; then
                CFLAGS="${CFLAGS} -DDEBUG"
                CXXFLAGS="${CXXFLAGS} -DDEBUG"
                OBJCFLAGS="${OBJCFLAGS} -DDEBUG"
                OBJCXXFLAGS="${OBJCXXFLAGS} -DDEBUG"
            fi
    ])
])
