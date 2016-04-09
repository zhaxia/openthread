OPENTHREAD_DIR=../../..

bin_PROGRAMS = thread-driver
thread_driver_SOURCES = main.cc cmdline.c serial_port.cc thread_driver.cc

if DARWIN
thread_driver_SOURCES += darwin/tun_netif.cc
endif

if LINUX
thread_driver_SOURCES += linux/tun_netif.cc
endif

thread_driver_CPPFLAGS = \
    -I$(OPENTHREAD_DIR)/include     \
    -I$(OPENTHREAD_DIR)/lib         \
    -I$(OPENTHREAD_DIR)/src         \
    -I$(OPENTHREAD_DIR)/test/src    \
    -I$(OPENTHREAD_DIR)/third_party
thread_driver_LDADD = ../../src/libthread.a -lpthread
