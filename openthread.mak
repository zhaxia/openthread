#
#    Copyright (c) 2016, Nest Labs, Inc.
#    All rights reserved.
#
#    Redistribution and use in source and binary forms, with or without
#    modification, are permitted provided that the following conditions are met:
#    1. Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#    2. Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#    3. Neither the name of the copyright holder nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
#    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

OPENTHREAD_CORE_SOURCES                                   = \
    src/core/openthread.cpp                                 \
    src/core/coap/coap_header.cpp                           \
    src/core/coap/coap_server.cpp                           \
    src/core/common/logging.cpp                             \
    src/core/common/message.cpp                             \
    src/core/common/tasklet.cpp                             \
    src/core/common/timer.cpp                               \
    src/core/crypto/aes_ccm.cpp                             \
    src/core/mac/mac.cpp                                    \
    src/core/mac/mac_frame.cpp                              \
    src/core/mac/mac_whitelist.cpp                          \
    src/core/net/icmp6.cpp                                  \
    src/core/net/ip6.cpp                                    \
    src/core/net/ip6_address.cpp                            \
    src/core/net/ip6_mpl.cpp                                \
    src/core/net/ip6_routes.cpp                             \
    src/core/net/netif.cpp                                  \
    src/core/net/udp6.cpp                                   \
    src/core/thread/address_resolver.cpp                    \
    src/core/thread/key_manager.cpp                         \
    src/core/thread/lowpan.cpp                              \
    src/core/thread/mesh_forwarder.cpp                      \
    src/core/thread/mle.cpp                                 \
    src/core/thread/mle_router.cpp                          \
    src/core/thread/mle_tlvs.cpp                            \
    src/core/thread/network_data.cpp                        \
    src/core/thread/network_data_local.cpp                  \
    src/core/thread/network_data_leader.cpp                 \
    src/core/thread/thread_netif.cpp                        \
    src/core/thread/thread_tlvs.cpp                         \
    third_party/mbedtls/mbedcrypto.c                        \
    third_party/mbedtls/repo/library/aes.c                  \
    third_party/mbedtls/repo/library/md.c                   \
    third_party/mbedtls/repo/library/md_wrap.c              \
    third_party/mbedtls/repo/library/memory_buffer_alloc.c  \
    third_party/mbedtls/repo/library/platform.c             \
    third_party/mbedtls/repo/library/sha256.c               \
    $(NULL)

OPENTHREAD_CORE_DEFINES                                   = \
    MBEDTLS_CONFIG_FILE=\"mbedtls-config.h\"                \
    $(NULL)

OPENTHREAD_CLI_SOURCES                                    = \
    src/cli/cli.cpp                                         \
    src/cli/cli_serial.cpp                                  \
    src/cli/cli_udp.cpp                                     \
    $(NULL)
