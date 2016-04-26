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

lib_LIBRARIES                       = libopenthread.a

libopenthread_a_CPPFLAGS =            \
    -I$(top_srcdir)/include           \
    -std=c++11                        \
    $(OPENTHREAD_TARGET_DEFINES)      \
    $(NULL)

libopenthread_a_SOURCES             = \
    openthread.cpp                    \
    coap/coap_header.cpp              \
    coap/coap_server.cpp              \
    common/message.cpp                \
    common/tasklet.cpp                \
    common/timer.cpp                  \
    crypto/aes_ccm.cpp                \
    crypto/aes_ecb.cpp                \
    crypto/hmac.cpp                   \
    crypto/sha256.cpp                 \
    mac/mac.cpp                       \
    mac/mac_frame.cpp                 \
    mac/mac_whitelist.cpp             \
    net/icmp6.cpp                     \
    net/ip6.cpp                       \
    net/ip6_address.cpp               \
    net/ip6_mpl.cpp                   \
    net/ip6_routes.cpp                \
    net/netif.cpp                     \
    net/udp6.cpp                      \
    thread/address_resolver.cpp       \
    thread/key_manager.cpp            \
    thread/lowpan.cpp                 \
    thread/mesh_forwarder.cpp         \
    thread/mle.cpp                    \
    thread/mle_router.cpp             \
    thread/mle_tlvs.cpp               \
    thread/network_data.cpp           \
    thread/network_data_local.cpp     \
    thread/network_data_leader.cpp    \
    thread/thread_netif.cpp           \
    thread/thread_tlvs.cpp            \
    $(NULL)

include_HEADERS                     = \
    $(NULL)

install-headers: install-includeHEADERS

if OPENTHREAD_BUILD_COVERAGE
CLEANFILES                          = $(wildcard *.gcda *.gcno)
endif # OPENTHREAD_BUILD_COVERAGE

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
