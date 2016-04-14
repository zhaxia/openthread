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
#      --enable-optimization configuration option to the package and
#      controls whether the package will be built with or without code
#      optimization.
#

#
# NL_ENABLE_OPTIMIZATION(default)
#
#   default - Whether the option should be enabled (yes) or disabled (no)
#             by default.
#
# Adds an --enable-optimization configuration option to the package with a
# default value of 'default' (should be either 'no' or 'yes') and controls
# whether the package will be built with or without code optimization.
#
# The value 'nl_cv_build_optimized' will be set to the result. In
# addition, the contents of CFLAGS, CXXFLAGS, OBJCFLAGS, and OBJCXXFLAGS may
# be altered by the use of this macro, converting -O<something> to -O0.
#
# NOTE: The behavior of this is influenced by nl_cv_build_coverage from
#       NL_ENABLE_COVERAGE
#
#------------------------------------------------------------------------------
AC_DEFUN([NL_ENABLE_OPTIMIZATION],
[
    # Check whether or not a default value has been passed in.

    m4_case([$1],
        [yes],[],
        [no],[],
        [m4_fatal([$0: invalid default value '$1'; must be 'yes' or 'no'])])

    AC_CACHE_CHECK([whether to build code-optimized instances of programs and libraries],
        nl_cv_build_optimized,
        [
            AC_ARG_ENABLE(optimization,
                [AS_HELP_STRING([--enable-optimization],[Enable the generation of code-optimized instances @<:@default=$1@:>@.])],
                [
                    case "${enableval}" in 

                    no|yes)
                        nl_cv_build_optimized=${enableval}

                        if test "${nl_cv_build_coverage}" = "yes"; then
                            AC_MSG_ERROR([both --enable-optimization and --enable-coverage cannot used. Please, choose one or the other to enable.])
                        fi
                        ;;

                    *)
                        AC_MSG_ERROR([Invalid value ${enableval} for --enable-optimized])
                        ;;

                    esac
                ],
                [
                    if test "${nl_cv_build_coverage}" = "yes"; then
                        AC_MSG_WARN([--enable-coverage was specified, optimization disabled])
                        nl_cv_build_optimized=no
            
                    else
                        nl_cv_build_optimized=$1
            
                    fi
                ])

            if test "${nl_cv_build_optimized}" = "no"; then
                CFLAGS="`echo ${CFLAGS} | sed -e 's,-O[[[:alnum:]]]*,-O0,g'`"
                CXXFLAGS="`echo ${CXXFLAGS} | sed -e 's,-O[[[:alnum:]]]*,-O0,g'`"
                OBJCFLAGS="`echo ${OBJCFLAGS} | sed -e 's,-O[[[:alnum:]]]*,-O0,g'`"
                OBJCXXFLAGS="`echo ${OBJCXXFLAGS} | sed -e 's,-O[[[:alnum:]]]*,-O0,g'`"
            fi
    ])
])
