#
#    Copyright (c) 2016 Nest Labs, Inc.
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
#      This file is the GNU automake template for the OpenThread.
#

include $(abs_top_nlbuild_autotools_dir)/automake/pre.am

lib_LIBRARIES                       = libopenthread.a

libopenthread_a_CPPFLAGS = \
    -I$(top_srcdir)/include           \
    $(NULL)

libopenthread_a_SOURCES  = openthread.c

include_HEADERS                     = \
    openthread.h           \
    $(NULL)

install-headers: install-includeHEADERS

if OPENTHREAD_BUILD_COVERAGE
CLEANFILES                          = $(wildcard *.gcda *.gcno)
endif # OPENTHREAD_BUILD_COVERAGE

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
