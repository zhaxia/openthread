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

# Always package (e.g. for 'make dist') these subdirectories.

DIST_SUBDIRS                            = \
    platform                              \
    ncp                                   \
    soc                                   \
    unit                                  \
    $(NULL)

# Always build (e.g. for 'make all') these subdirectories.

SUBDIRS                                 = \
    platform                              \
    ncp                                   \
    soc                                   \
    unit                                  \
    $(NULL)

all:

if OPENTHREAD_BUILD_TESTS
if OPENTHREAD_BUILD_COVERAGE
CLEANFILES                             = $(wildcard *.gcda *.gcno)

if OPENTHREAD_BUILD_COVERAGE_REPORTS
# The bundle should positively be qualified with the absolute build
# path. Otherwise, VPATH will get auto-prefixed to it if there is
# already such a directory in the non-colocated source tree.

OPENTHREAD_COVERAGE_BUNDLE             = ${abs_builddir}/${PACKAGE}${NL_COVERAGE_BUNDLE_SUFFIX}
OPENTHREAD_COVERAGE_INFO               = ${OPENTHREAD_COVERAGE_BUNDLE}/${PACKAGE}${NL_COVERAGE_INFO_SUFFIX}

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
