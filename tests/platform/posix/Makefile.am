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

lib_LIBRARIES                          = libopenthread-posix.a

libopenthread_posix_a_CPPFLAGS         = \
    -I$(top_srcdir)/include              \
    -I$(top_srcdir)/src                  \
    -I$(top_srcdir)/src/core             \
    -I$(top_srcdir)/tests                \
    -I$(top_srcdir)/tests/platform/posix \
    $(OPENTHREAD_TARGET_DEFINES)         \
    -D_BSD_SOURCE                        \
    -D_GNU_SOURCE                        \
    $(NULL)

libopenthread_posix_a_CXXFLAGS         = \
    -I$(top_srcdir)/include              \
    -I$(top_srcdir)/src                  \
    -I$(top_srcdir)/src/core             \
    -I$(top_srcdir)/tests                \
    -I$(top_srcdir)/tests/platform/posix \
    $(OPENTHREAD_TARGET_DEFINES)         \
    $(NULL)

libopenthread_posix_a_SOURCES          = \
    alarm.c                              \
    atomic.c                             \
    cli_posix.cpp                        \
    cmdline.c                            \
    logging.c                            \
    radio.cpp                            \
    random.c                             \
    serial.c                             \
    $(NULL)

include_HEADERS                        = \
    $(NULL)

install-headers: install-includeHEADERS

if OPENTHREAD_BUILD_COVERAGE
CLEANFILES                          = $(wildcard *.gcda *.gcno)
endif # OPENTHREAD_BUILD_COVERAGE

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
