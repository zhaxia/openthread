/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

OPENTHREAD_SOURCES = \
    coap/coap_header.cc           \
    coap/coap_server.cc           \
    common/message.cc             \
    common/random.cc              \
    common/tasklet.cc             \
    common/timer.cc               \
    crypto/aes_ccm.cc             \
    crypto/aes_ecb.cc             \
    crypto/hmac.cc                \
    crypto/sha256.cc              \
    mac/mac.cc                    \
    mac/mac_frame.cc              \
    mac/mac_whitelist.cc          \
    ncp/hdlc.cc                   \
    ncp/ncp.cc                    \
    ncp/ncp_base.cc               \
    ncp/ncp.pb-c.c                \
    net/icmp6.cc                  \
    net/ip6.cc                    \
    net/ip6_address.cc            \
    net/ip6_mpl.cc                \
    net/ip6_routes.cc             \
    net/netif.cc                  \
    net/udp6.cc                   \
    thread/address_resolver.cc    \
    thread/key_manager.cc         \
    thread/lowpan.cc              \
    thread/mesh_forwarder.cc      \
    thread/mle.cc                 \
    thread/mle_router.cc          \
    thread/mle_tlvs.cc            \
    thread/network_data.cc        \
    thread/network_data_local.cc  \
    thread/network_data_leader.cc \
    thread/thread_netif.cc        \
    thread/thread_tlvs.cc         \
    $(NULL)

OPENTHREAD_LIB_CLI_SOURCES =      \
    cli/cli_command.cc            \
    cli/cli_ifconfig.cc           \
    cli/cli_ip.cc                 \
    cli/cli_mac.cc                \
    cli/cli_netdata.cc            \
    cli/cli_ping.cc               \
    cli/cli_route.cc              \
    cli/cli_serial.cc             \
    cli/cli_server.cc             \
    cli/cli_shutdown.cc           \
    cli/cli_thread.cc             \
    cli/cli_udp.cc                \
    $(NULL)

OPENTHREAD_LIB_PROTOBUF_SOURCES = \
    protobuf/protobuf-c.c         \
    $(NULL)

