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

OPENTHREAD_CORE_SOURCES                   = \
    src/core/openthread.cpp                 \
    src/core/coap/coap_header.cpp           \
    src/core/coap/coap_server.cpp           \
    src/core/common/message.cpp             \
    src/core/common/tasklet.cpp             \
    src/core/common/timer.cpp               \
    src/core/crypto/aes_ccm.cpp             \
    src/core/crypto/aes_ecb.cpp             \
    src/core/crypto/hmac.cpp                \
    src/core/crypto/sha256.cpp              \
    src/core/mac/mac.cpp                    \
    src/core/mac/mac_frame.cpp              \
    src/core/mac/mac_whitelist.cpp          \
    src/core/net/icmp6.cpp                  \
    src/core/net/ip6.cpp                    \
    src/core/net/ip6_address.cpp            \
    src/core/net/ip6_mpl.cpp                \
    src/core/net/ip6_routes.cpp             \
    src/core/net/netif.cpp                  \
    src/core/net/udp6.cpp                   \
    src/core/thread/address_resolver.cpp    \
    src/core/thread/key_manager.cpp         \
    src/core/thread/lowpan.cpp              \
    src/core/thread/mesh_forwarder.cpp      \
    src/core/thread/mle.cpp                 \
    src/core/thread/mle_router.cpp          \
    src/core/thread/mle_tlvs.cpp            \
    src/core/thread/network_data.cpp        \
    src/core/thread/network_data_local.cpp  \
    src/core/thread/network_data_leader.cpp \
    src/core/thread/thread_netif.cpp        \
    src/core/thread/thread_tlvs.cpp         \
    $(NULL)

OPENTHREAD_NCP_SOURCES                    = \
    src/ncp/hdlc.cpp                        \
    src/ncp/ncp.cpp                         \
    src/ncp/ncp_base.cpp                    \
    src/ncp/ncp.pb-c.c                      \
    $(NULL)

OPENTHREAD_CLI_SOURCES                    = \
    src/cli/cli_command.cpp                 \
    src/cli/cli_ifconfig.cpp                \
    src/cli/cli_ip.cpp                      \
    src/cli/cli_mac.cpp                     \
    src/cli/cli_netdata.cpp                 \
    src/cli/cli_ping.cpp                    \
    src/cli/cli_route.cpp                   \
    src/cli/cli_serial.cpp                  \
    src/cli/cli_server.cpp                  \
    src/cli/cli_shutdown.cpp                \
    src/cli/cli_thread.cpp                  \
    src/cli/cli_udp.cpp                     \
    $(NULL)

OPENTHREAD_PROTOBUF_SOURCES               = \
    third_party/protobuf/protobuf-c.c       \
    $(NULL)
