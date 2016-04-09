
noinst_LIBRARIES = libthread.a
libthread_a_SOURCES = \
    ../../lib/cli/cli_command.cc \
    ../../lib/cli/cli_ifconfig.cc \
    ../../lib/cli/cli_ip.cc \
    ../../lib/cli/cli_mac.cc \
    ../../lib/cli/cli_netdata.cc \
    ../../lib/cli/cli_ping.cc \
    ../../lib/cli/cli_route.cc \
    ../../lib/cli/cli_server.cc \
    ../../lib/cli/cli_shutdown.cc \
    ../../lib/cli/cli_thread.cc \
    ../../lib/cli/cli_udp.cc \
    ../../src/coap/coap_header.cc \
    ../../src/coap/coap_server.cc \
    ../../src/common/message.cc \
    ../../src/common/random.cc \
    ../../src/common/tasklet.cc \
    ../../src/common/timer.cc \
    ../../src/crypto/aes_ccm.cc \
    ../../src/crypto/aes_ecb.cc \
    ../../src/crypto/hmac.cc \
    ../../src/crypto/sha256.cc \
    ../../src/mac/mac.cc \
    ../../src/mac/mac_frame.cc \
    ../../src/mac/mac_whitelist.cc \
    ../../src/ncp/hdlc.cc \
    ../../src/ncp/ncp_base.cc \
    ../../src/ncp/ncp.cc \
    ../../src/ncp/ncp.pb-c.c \
    ../../src/net/icmp6.cc \
    ../../src/net/ip6.cc \
    ../../src/net/ip6_address.cc \
    ../../src/net/ip6_mpl.cc \
    ../../src/net/ip6_routes.cc \
    ../../src/net/netif.cc \
    ../../src/net/udp6.cc \
    ../../src/thread/address_resolver.cc \
    ../../src/thread/key_manager.cc \
    ../../src/thread/lowpan.cc \
    ../../src/thread/mesh_forwarder.cc \
    ../../src/thread/mle.cc \
    ../../src/thread/mle_router.cc \
    ../../src/thread/mle_tlvs.cc \
    ../../src/thread/network_data.cc \
    ../../src/thread/network_data_local.cc \
    ../../src/thread/network_data_leader.cc \
    ../../src/thread/thread_netif.cc \
    ../../src/thread/thread_tlvs.cc \
    ../../third_party/protobuf/protobuf-c.c \
    platform/posix/alarm.cc \
    platform/posix/atomic.cc \
    platform/posix/cli_posix.cc \
    platform/posix/cmdline.c \
    platform/posix/phy.cc \
    platform/posix/sleep.cc \
    platform/posix/uart.cc \
    $(NULL)

CXXFLAGS += -DAUTOMAKE

CPPFLAGS += -I../../include -I../../lib -I../../src -I../../third_party
