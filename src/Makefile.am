
noinst_LIBRARIES = libthread.a
libthread_a_SOURCES = \
    cli/cli_command.cc \
    cli/cli_ifconfig.cc \
    cli/cli_ip.cc \
    cli/cli_mac.cc \
    cli/cli_netdata.cc \
    cli/cli_ping.cc \
    cli/cli_route.cc \
    cli/cli_server.cc \
    cli/cli_shutdown.cc \
    cli/cli_thread.cc \
    cli/cli_udp.cc \
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
    ncp/hdlc.cc \
    ncp/ncp_base.cc \
    ncp/ncp.cc \
    ncp/ncp.pb-c.c \
    net/icmp6.cc \
    net/ip6.cc \
    net/ip6_address.cc \
    net/ip6_mpl.cc \
    net/ip6_routes.cc \
    net/netif.cc \
    net/udp6.cc \
    platform/posix/alarm.cc \
    platform/posix/atomic.cc \
    platform/posix/cli_posix.cc \
    platform/posix/cmdline.c \
    platform/posix/phy.cc \
    platform/posix/sleep.cc \
    platform/posix/uart.cc \
    protobuf/protobuf-c.c \
    thread/address_resolver.cc \
    thread/key_manager.cc \
    thread/lowpan.cc \
    thread/mesh_forwarder.cc \
    thread/mle.cc \
    thread/mle_router.cc \
    thread/mle_tlvs.cc \
    thread/network_data.cc \
    thread/network_data_local.cc \
    thread/network_data_leader.cc \
    thread/thread_netif.cc \
    thread/thread_tlvs.cc \
    $(NULL)

CXXFLAGS += -DAUTOMAKE

CPPFLAGS += -I ../include
