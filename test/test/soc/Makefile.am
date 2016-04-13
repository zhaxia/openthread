OPENTHREAD_DIR=../../..

bin_PROGRAMS = soc
soc_SOURCES = main.cpp
soc_CPPFLAGS = \
    -I$(OPENTHREAD_DIR)/include     \
    -I$(OPENTHREAD_DIR)/src         \
    -I$(OPENTHREAD_DIR)/src/core    \
    -I$(OPENTHREAD_DIR)/test/src    \
    -I$(OPENTHREAD_DIR)/third_party
soc_LDADD = ../../src/libthread.a  -lpthread
