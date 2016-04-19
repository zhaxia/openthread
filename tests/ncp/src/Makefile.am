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

bin_PROGRAMS = ncp

ncp_CPPFLAGS                                   = \
    -I$(top_srcdir)/include                      \
    -I$(top_srcdir)/src                          \
    -I$(top_srcdir)/src/core                     \
    -I$(top_srcdir)/tests                        \
    -I$(top_srcdir)/third_party                  \
    -std=c++11                                   \
    $(OPENTHREAD_TARGET_DEFINES)                 \
    $(NULL)

ncp_LDADD                                      = \
    $(top_srcdir)/src/ncp/libopenthread-ncp.a    \
    $(top_srcdir)/src/core/libopenthread.a       \
    $(top_srcdir)/tests/platform/posix/libopenthread-posix.a            \
    -lpthread                                    \
    $(NULL)

ncp_SOURCES = main.cpp

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
