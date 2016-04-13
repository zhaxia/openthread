
noinst_LIBRARIES = libthread.a
libthread_a_SOURCES = \
    ../../src/core/coap/coap_header.cpp \
    ../../src/core/coap/coap_server.cpp \
    ../../src/core/common/message.cpp \
    ../../src/core/common/random.cpp \
    ../../src/core/common/tasklet.cpp \
    ../../src/core/common/timer.cpp \
    ../../src/core/crypto/aes_ccm.cpp \
    ../../src/core/crypto/aes_ecb.cpp \
    ../../src/core/crypto/hmac.cpp \
    ../../src/core/crypto/sha256.cpp \
    ../../src/core/mac/mac.cpp \
    ../../src/core/mac/mac_frame.cpp \
    ../../src/core/mac/mac_whitelist.cpp \
    ../../src/core/net/icmp6.cpp \
    ../../src/core/net/ip6.cpp \
    ../../src/core/net/ip6_address.cpp \
    ../../src/core/net/ip6_mpl.cpp \
    ../../src/core/net/ip6_routes.cpp \
    ../../src/core/net/netif.cpp \
    ../../src/core/net/udp6.cpp \
    ../../src/core/thread/address_resolver.cpp \
    ../../src/core/thread/key_manager.cpp \
    ../../src/core/thread/lowpan.cpp \
    ../../src/core/thread/mesh_forwarder.cpp \
    ../../src/core/thread/mle.cpp \
    ../../src/core/thread/mle_router.cpp \
    ../../src/core/thread/mle_tlvs.cpp \
    ../../src/core/thread/network_data.cpp \
    ../../src/core/thread/network_data_local.cpp \
    ../../src/core/thread/network_data_leader.cpp \
    ../../src/core/thread/thread_netif.cpp \
    ../../src/core/thread/thread_tlvs.cpp \
    ../../src/cli/cli_command.cpp \
    ../../src/cli/cli_ifconfig.cpp \
    ../../src/cli/cli_ip.cpp \
    ../../src/cli/cli_mac.cpp \
    ../../src/cli/cli_netdata.cpp \
    ../../src/cli/cli_ping.cpp \
    ../../src/cli/cli_route.cpp \
    ../../src/cli/cli_server.cpp \
    ../../src/cli/cli_shutdown.cpp \
    ../../src/cli/cli_thread.cpp \
    ../../src/cli/cli_udp.cpp \
    ../../src/ncp/hdlc.cpp \
    ../../src/ncp/ncp.cpp \
    ../../src/ncp/ncp_base.cpp \
    ../../src/ncp/ncp.pb-c.c \
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

CPPFLAGS += -I../../include -I../../src -I../../src/core -I../../third_party
