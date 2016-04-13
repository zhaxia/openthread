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

OPENTHREAD_SOURCES               = \
    coap/coap_header.cpp           \
    coap/coap_server.cpp           \
    common/message.cpp             \
    common/random.cpp              \
    common/tasklet.cpp             \
    common/timer.cpp               \
    crypto/aes_ccm.cpp             \
    crypto/aes_ecb.cpp             \
    crypto/hmac.cpp                \
    crypto/sha256.cpp              \
    mac/mac.cpp                    \
    mac/mac_frame.cpp              \
    mac/mac_whitelist.cpp          \
    ncp/hdlc.cpp                   \
    ncp/ncp.cpp                    \
    ncp/ncp_base.cpp               \
    ncp/ncp.pb-c.c                 \
    net/icmp6.cpp                  \
    net/ip6.cpp                    \
    net/ip6_address.cpp            \
    net/ip6_mpl.cpp                \
    net/ip6_routes.cpp             \
    net/netif.cpp                  \
    net/udp6.cpp                   \
    thread/address_resolver.cpp    \
    thread/key_manager.cpp         \
    thread/lowpan.cpp              \
    thread/mesh_forwarder.cpp      \
    thread/mle.cpp                 \
    thread/mle_router.cpp          \
    thread/mle_tlvs.cpp            \
    thread/network_data.cpp        \
    thread/network_data_local.cpp  \
    thread/network_data_leader.cpp \
    thread/thread_netif.cpp        \
    thread/thread_tlvs.cpp         \
    $(NULL)

OPENTHREAD_LIB_CLI_SOURCES       = \
    cli/cli_command.cpp            \
    cli/cli_ifconfig.cpp           \
    cli/cli_ip.cpp                 \
    cli/cli_mac.cpp                \
    cli/cli_netdata.cpp            \
    cli/cli_ping.cpp               \
    cli/cli_route.cpp              \
    cli/cli_serial.cpp             \
    cli/cli_server.cpp             \
    cli/cli_shutdown.cpp           \
    cli/cli_thread.cpp             \
    cli/cli_udp.cpp                \
    $(NULL)

OPENTHREAD_LIB_PROTOBUF_SOURCES = \
    protobuf/protobuf-c.c         \
    $(NULL)

