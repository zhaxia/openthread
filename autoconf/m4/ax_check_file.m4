#
#    Copyright (c) 2014 Nest Labs, Inc.
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
#      This file implements a GNU M4 autoconf macro for checking for
#      the existence of files.
#
#      The autoconf version of AC_CHECK_FILE is absolutely broken in
#      that it cannot check for files when cross-compiling even though
#      the only thing it relies upon is a shell file readability
#      check.
#

# AX_CHECK_FILE(FILE, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# -------------------------------------------------------------
#
# Check for the existence of FILE.
AC_DEFUN([AX_CHECK_FILE],
[AS_VAR_PUSHDEF([ac_File], [ac_cv_file_$1])dnl
AC_CACHE_CHECK([for $1], [ac_File],
[if test -r "$1"; then
  AS_VAR_SET([ac_File], [yes])
else
  AS_VAR_SET([ac_File], [no])
fi])
AS_VAR_IF([ac_File], [yes], [$2], [$3])
AS_VAR_POPDEF([ac_File])dnl
])# AX_CHECK_FILE
