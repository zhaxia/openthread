
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
    $(NULL)

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

