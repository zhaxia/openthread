
noinst_LIBRARIES = libthread.a
libthread_a_SOURCES = \
    ../../lib/cli/cli_command.cpp \
    ../../lib/cli/cli_ifconfig.cpp \
    ../../lib/cli/cli_ip.cpp \
    ../../lib/cli/cli_mac.cpp \
    ../../lib/cli/cli_netdata.cpp \
    ../../lib/cli/cli_ping.cpp \
    ../../lib/cli/cli_route.cpp \
    ../../lib/cli/cli_server.cpp \
    ../../lib/cli/cli_shutdown.cpp \
    ../../lib/cli/cli_thread.cpp \
    ../../lib/cli/cli_udp.cpp \
    ../../src/coap/coap_header.cpp \
    ../../src/coap/coap_server.cpp \
    ../../src/common/message.cpp \
    ../../src/common/random.cpp \
    ../../src/common/tasklet.cpp \
    ../../src/common/timer.cpp \
    ../../src/crypto/aes_ccm.cpp \
    ../../src/crypto/aes_ecb.cpp \
    ../../src/crypto/hmac.cpp \
    ../../src/crypto/sha256.cpp \
    ../../src/mac/mac.cpp \
    ../../src/mac/mac_frame.cpp \
    ../../src/mac/mac_whitelist.cpp \
    ../../src/ncp/hdlc.cpp \
    ../../src/ncp/ncp.cpp \
    ../../src/ncp/ncp_base.cpp \
    ../../src/ncp/ncp.pb-c.c \
    ../../src/net/icmp6.cpp \
    ../../src/net/ip6.cpp \
    ../../src/net/ip6_address.cpp \
    ../../src/net/ip6_mpl.cpp \
    ../../src/net/ip6_routes.cpp \
    ../../src/net/netif.cpp \
    ../../src/net/udp6.cpp \
    ../../src/thread/address_resolver.cpp \
    ../../src/thread/key_manager.cpp \
    ../../src/thread/lowpan.cpp \
    ../../src/thread/mesh_forwarder.cpp \
    ../../src/thread/mle.cpp \
    ../../src/thread/mle_router.cpp \
    ../../src/thread/mle_tlvs.cpp \
    ../../src/thread/network_data.cpp \
    ../../src/thread/network_data_local.cpp \
    ../../src/thread/network_data_leader.cpp \
    ../../src/thread/thread_netif.cpp \
    ../../src/thread/thread_tlvs.cpp \
    ../../third_party/protobuf/protobuf-c.c \
    platform/posix/alarm.cpp \
    platform/posix/atomic.cpp \
    platform/posix/cli_posix.cpp \
    platform/posix/cmdline.c \
    platform/posix/phy.cpp \
    platform/posix/sleep.cpp \
    platform/posix/uart.cpp \
    $(NULL)

CXXFLAGS += -DAUTOMAKE

CPPFLAGS += -I../../include -I../../lib -I../../src -I../../third_party
