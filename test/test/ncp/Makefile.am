OPENTHREAD_DIR=../../..

bin_PROGRAMS = ncp
ncp_SOURCES = main.cpp
ncp_CPPFLAGS = \
    -I$(OPENTHREAD_DIR)/include     \
    -I$(OPENTHREAD_DIR)/src         \
    -I$(OPENTHREAD_DIR)/src/core    \
    -I$(OPENTHREAD_DIR)/test/src    \
    -I$(OPENTHREAD_DIR)/third_party
ncp_LDADD = ../../src/libthread.a -lpthread
