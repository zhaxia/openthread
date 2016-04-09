OPENTHREAD_DIR=../../..

bin_PROGRAMS = thread-ctl
thread_ctl_SOURCES = main.cc cmdline.c
thread_ctl_CPPFLAGS = \
    -I$(OPENTHREAD_DIR)/include     \
    -I$(OPENTHREAD_DIR)/lib         \
    -I$(OPENTHREAD_DIR)/src         \
    -I$(OPENTHREAD_DIR)/test/src    \
    -I$(OPENTHREAD_DIR)/third_party
thread_ctl_LDADD = ../../src/libthread.a
