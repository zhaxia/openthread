#
#    Copyright (c) 2016 Nest Labs, Inc.
#    All rights reserved.
#
#    This document is the property of Nest Labs. It is considered
#    confidential and proprietary information.
#
#    This document may not be reproduced or transmitted in any form,
#    in whole or in part, without the express written permission of
#    Nest Labs.
#
#    Description:
#      This is a make file for OpenThread library.
#

OPENTHREAD_INCLUDES = \
    coap/coap_header.h \
    coap/coap_server.h \
    common/encoding.h \
    common/message.h \
    common/random.h \
    common/timer.h \
    common/thread_error.h \
    crypto/aes.h \
    crypto/aes_ecb.h \
    mac/mac.h \
    mac/mac_frame.h \
    mac/mac_whitelist.h \
    net/icmp6.h \
    net/ip6.h \
    net/ip6_address.h \
    net/netif.h \
    net/socket.h \
    net/udp6.h \
    thread/address_resolver.h \
    thread/key_manager.h \
    thread/lowpan.h \
    thread/mesh_forwarder.h \
    thread/mle.h \
    thread/mle_tlvs.h \
    thread/mle_router.h \
    thread/network_data.h \
    thread/network_data_leader.h \
    thread/network_data_local.h \
    thread/network_data_tlvs.h \
    thread/thread_netif.h \
    thread/thread_tlvs.h \
    thread/topology.h \
    platform/common/alarm.h \
    platform/common/alarm_interface.h \
    platform/common/phy.h \
    platform/common/phy_interface.h \
    platform/common/sleep.h \
    platform/common/uart.h \
    platform/common/uart_interface.h \


# The minimum set of files needed for open thread

OPENTHREAD_SOURCES = \
    coap/coap_header.cc \
    coap/coap_server.cc \
    common/message.cc \
    common/random.cc \
    common/tasklet.cc \
    common/timer.cc \
    crypto/aes_ccm.cc \
    crypto/aes_ecb.cc \
    crypto/hmac.cc \
    crypto/sha256.cc \
    mac/mac.cc \
    mac/mac_frame.cc \
    mac/mac_whitelist.cc \
    net/icmp6.cc \
    net/ip6.cc \
    net/ip6_address.cc \
    net/ip6_mpl.cc \
    net/ip6_routes.cc \
    net/netif.cc \
    net/udp6.cc \
    ncp/hdlc.cc \
    ncp/ncp.cc \
    ncp/ncp.pb-c.c \
    protobuf/protobuf-c.c \
    thread/address_resolver.cc \
    thread/key_manager.cc \
    thread/lowpan.cc \
    thread/mesh_forwarder.cc \
    thread/mle.cc \
    thread/mle_tlvs.cc \
    thread/mle_router.cc \
    thread/network_data.cc \
    thread/network_data_local.cc \
    thread/network_data_leader.cc \
    thread/thread_netif.cc \
    thread/thread_tlvs.cc \
    tun/tun_netif.cc \
    $(NULL)


OPENTHREAD_SOURCES_VNCP = \
    $(NULL)


OPENTHREAD_SOURCES_NCP = \
    $(NULL)

