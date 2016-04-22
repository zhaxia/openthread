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

DIST_SUBDIRS                                   = \
    scripts                                      \
    src                                          \
    $(NULL)

# Always build (e.g. for 'make all') these subdirectories.

SUBDIRS                                        = \
    src                                          \
    $(NULL)

all:

if OPENTHREAD_BUILD_TESTS

# First, list all essential program and script tests that MUST be run.

check_PROGRAMS =

check_SCRIPTS                                  = \
    scripts/Cert_5_1_01_RouterAttach.py          \
    $(NULL)

# List all other non-essential program and script tests that MAY be run.

if !OPENTHREAD_WITH_MINIMUM_SET_OF_TESTS

check_SCRIPTS                                 += \
    $(NULL)

endif # OPENTHREAD_WITH_ESSENTIAL_TESTS_ONLY

TESTS_ENVIRONMENT                              = \
    export                                       \
    abs_builddir='$(abs_builddir)'               \
    abs_srcdir='$(abs_srcdir)';                  \
    $(NULL)

TESTS                                          = \
    $(check_PROGRAMS)                            \
    $(check_SCRIPTS)                             \
    $(NULL)

endif # OPENTHREAD_BUILD_TESTS

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
