#
#    Copyright (c) 2014-2015 Nest Labs, Inc.
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
#      This file defines a number of GNU autoconf M4-style macros
#      for checking language-specific compiler options.
#

#
# _AX_CHECK_COMPILER_OPTION_WITH_VAR(language, variable, option)
#
#   language - The autoconf language (C, C++, Objective C, Objective C++,
#              etc.).
#   variable - The variable to add the checked compiler option to.
#   option   - The compiler flag to check.
#
# Add, if supported, the specified compiler flag for the compiler selected
# for the specified language to the provided variable.
# ----------------------------------------------------------------------------
AC_DEFUN([_AX_CHECK_COMPILER_OPTION_WITH_VAR],
[
    AC_LANG_PUSH($1)
    AC_MSG_CHECKING([whether the _AC_LANG compiler understands $3])
    SAVE_[]_AC_LANG_PREFIX[]FLAGS=${_AC_LANG_PREFIX[]FLAGS}
    SAVE_$2=${$2}
    _AC_LANG_PREFIX[]FLAGS=$3
    AC_TRY_COMPILE(,[;],AC_MSG_RESULT([yes]); _AC_LANG_PREFIX[]FLAGS="${SAVE_[]_AC_LANG_PREFIX[]FLAGS}"; $2="${SAVE_$2} $3",AC_MSG_RESULT([no]); _AC_LANG_PREFIX[]FLAGS=${SAVE_[]_AC_LANG_PREFIX[]FLAGS}; $2=${SAVE_$2});
    unset SAVE_[]_AC_LANG_PREFIX[]FLAGS
    unset SAVE_$2
    AC_LANG_POP($1)
])

#
# _AX_CHECK_COMPILER_OPTION(language, option)
#
#   language - The autoconf language (C, C++, Objective C, Objective C++,
#              etc.).
#   option   - The compiler flag to check.
#
# Add, if supported, the specified compiler flag for the compiler selected
# for the specified language.
# ----------------------------------------------------------------------------
AC_DEFUN([_AX_CHECK_COMPILER_OPTION],
[
    AC_LANG_PUSH($1)
    AC_MSG_CHECKING([whether the _AC_LANG compiler understands $2])
    SAVE_[]_AC_LANG_PREFIX[]FLAGS=${_AC_LANG_PREFIX[]FLAGS}
    _AC_LANG_PREFIX[]FLAGS=$2
    AC_TRY_COMPILE(,[;],AC_MSG_RESULT([yes]); _AC_LANG_PREFIX[]FLAGS="${SAVE_[]_AC_LANG_PREFIX[]FLAGS} $2",AC_MSG_RESULT([no]); _AC_LANG_PREFIX[]FLAGS=${SAVE_[]_AC_LANG_PREFIX[]FLAGS});
    unset SAVE_[]_AC_LANG_PREFIX[]FLAGS
    AC_LANG_POP($1)
])

#
# AX_CHECK_COMPILER_OPTION(language, [variable,] option)
#
#   language - The autoconf language (C, C++, Objective C, Objective C++,
#              etc.).
#   variable - If supplied, the variable to add the checked compiler option
#              to.
#   option   - The compiler flag to check.
#
# Add, if supported, the specified compiler flag for the compiler selected
# for the specified language, optionally saving it to the specified variable.
# ----------------------------------------------------------------------------
AC_DEFUN([AX_CHECK_COMPILER_OPTION],
[
    ifelse($#,
        3,
        [_AX_CHECK_COMPILER_OPTION_WITH_VAR($1, $2, $3)],
        [_AX_CHECK_COMPILER_OPTION($1, $2)])
])

#
# AX_CHECK_COMPILER_OPTIONS(language, [variable,] option ...)
#
#   language - The autoconf language (C, C++, Objective C, Objective C++,
#              etc.).
#   variable - If supplied, the variable to add the checked compiler option
#              to.
#   options  - The compiler flags to check.
#
# Add, if supported, the specified compiler flags for the compiler selected
# for the specified language, optionally saving it to the specified variable.
# ----------------------------------------------------------------------------
AC_DEFUN([AX_CHECK_COMPILER_OPTIONS],
[
    ifelse($#,
        3,
        [
            for ax_compiler_option in [$3]; do
                _AX_CHECK_COMPILER_OPTION_WITH_VAR([$1], [$2], $ax_compiler_option)
            done
	],
        [
            for ax_compiler_option in [$2]; do
                _AX_CHECK_COMPILER_OPTION([$1], $ax_compiler_option)
            done
	])
])
