#
#    Copyright 2016 Nest Labs Inc. All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

include $(abs_top_nlbuild_autotools_dir)/automake/pre.am

#
# Local headers to build against and distribute but not to install
# since they are not part of the package.
#
noinst_HEADERS                                 = \
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
    -I$(top_srcdir)/src                          \
    -I$(top_srcdir)/src/core                     \
    $(NULL)

COMMON_LDADD                                   = \
    $(top_srcdir)/src/core/libopenthread.a       \
    $(top_srcdir)/tests/platform/posix/libopenthread-posix.a       \
    $(NULL)

# Test applications that should be run when the 'check' target is run.

check_PROGRAMS                                 = \
    test-aes                                     \
    test-hmac-sha256                             \
    test-mac-frame                               \
    test-message                                 \
    test-sha256                                  \
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

test_aes_LDADD               = $(COMMON_LDADD)
test_aes_SOURCES             = test_aes.cpp

test_hmac_sha256_LDADD       = $(COMMON_LDADD)
test_hmac_sha256_SOURCES     = test_hmac_sha256.cpp

test_mac_frame_LDADD         = $(COMMON_LDADD)
test_mac_frame_SOURCES       = test_mac_frame.cpp

test_message_LDADD           = $(COMMON_LDADD)
test_message_SOURCES         = test_message.cpp

test_sha256_LDADD            = $(COMMON_LDADD)
test_sha256_SOURCES          = test_sha256.cpp

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
