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
#      This file is the GNU automake template for the OpenThread
#      unit tests.
#

include $(abs_top_nlbuild_autotools_dir)/automake/pre.am

#
# Local headers to build against and distribute but not to install
# since they are not part of the package.
#
noinst_HEADERS                                 = \
    openthread-test.h                 \
    $(NULL)

#
# Other files we do want to distribute with the package.
#
EXTRA_DIST                                     = \
    $(NULL)

if OPENTHREAD_BUILD_TESTS
# C preprocessor option flags that will apply to all compiled objects in this
# makefile.

AM_CPPFLAGS                                    = \
    -I$(top_srcdir)/include                      \
    $(NULL)

COMMON_LDADD                                   = \
    -L${top_builddir}/src -lopenthread
    $(NULL)

# Test applications that should be run when the 'check' target is run.

check_PROGRAMS                                 = \
    openthread-test                   \
    openthread-test-cxx               \
    $(NULL)

# Test applications and scripts that should be built and run when the
# 'check' target is run.

TESTS                                          = \
    $(check_PROGRAMS)                            \
    $(NULL)

# The additional environment variables and their values that will be
# made available to all programs and scripts in TESTS.

TESTS_ENVIRONMENT                              = \
    $(NULL)

# Source, compiler, and linker options for test programs.

openthread_test_LDADD               = $(COMMON_LDADD)
openthread_test_SOURCES             = openthread-test.c                 

openthread_test_cxx_LDADD           = $(COMMON_LDADD)
openthread_test_cxx_SOURCES         = openthread-test-cxx.cpp           

if OPENTHREAD_BUILD_COVERAGE
CLEANFILES                                     = $(wildcard *.gcda *.gcno)

if OPENTHREAD_BUILD_COVERAGE_REPORTS
# The bundle should positively be qualified with the absolute build
# path. Otherwise, VPATH will get auto-prefixed to it if there is
# already such a directory in the non-colocated source tree.

OPENTHREAD_COVERAGE_BUNDLE                           = ${abs_builddir}/${PACKAGE}${NL_COVERAGE_BUNDLE_SUFFIX}
OPENTHREAD_COVERAGE_INFO                             = ${OPENTHREAD_COVERAGE_BUNDLE}/${PACKAGE}${NL_COVERAGE_INFO_SUFFIX}

$(OPENTHREAD_COVERAGE_BUNDLE):
	$(call create-directory)

$(OPENTHREAD_COVERAGE_INFO): check | $(dir $(OPENTHREAD_COVERAGE_INFO))
	$(call generate-coverage-report,${top_builddir})

coverage: $(OPENTHREAD_COVERAGE_INFO)

clean-local: clean-local-coverage

.PHONY: clean-local-coverage
clean-local-coverage:
	-$(AM_V_at)rm -rf $(OPENTHREAD_COVERAGE_BUNDLE)
endif # OPENTHREAD_BUILD_COVERAGE_REPORTS
endif # OPENTHREAD_BUILD_COVERAGE
endif # OPENTHREAD_BUILD_TESTS

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
