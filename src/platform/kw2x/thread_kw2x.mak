#
#    Copyright (c) 2010 Nest Labs, Inc.
#    All rights reserved.
#
#    This document is the property of Nest Labs, Inc. It is
#    considered confidential and proprietary information.
#
#    This document may not be reproduced or transmitted in any form,
#    in whole or in part, without the express written permission of
#    Nest Labs, Inc.
#
#    Description:
#      This file is the top-level make file for the project.
#

include pre.mak

TARGET = thread_kw2x

ARCHIVES = $(TARGET)

VPATH = \
      	$(BspRoot)/MacPhy/Phy/Source \
	$(BspRoot)/PLM/Source/Common/MC1324xDrv \
	$(BuildRoot)/src/lib/thread/src/platform/cortex-m

# Define SOURCES, INCLUDES, DEFINES according to Nest make macro naming:
# <module>_SOURCES, where <module> is one of
# PROGRAMS, IMAGES, ARCHIVES, LIBRARIES

$(TARGET)_SOURCES = \
	$(BspRoot)/MacPhy/Phy/Source/PhyISR.c \
	$(BspRoot)/MacPhy/Phy/Source/PhyPacketProcessor.c \
	$(BspRoot)/MacPhy/Phy/Source/PhyPlmeData.c \
	$(BspRoot)/MacPhy/Phy/Source/PhyTime.c \
	$(BspRoot)/PLM/Source/Common/MC1324xDrv/MC1324xDrv.c \
	$(BuildRoot)/src/lib/thread/src/platform/cortex-m/atomic.cc \
	$(BuildRoot)/src/lib/thread/src/platform/cortex-m/sleep.cc \
	alarm.cc \
	phy.cc \
	uart.cc

$(TARGET)_INCLUDES = \
	$(BuildRoot)/src/lib/thread/src		\
	$(BspRoot)/MacPhy/Interface		\
	$(BspRoot)/MacPhy/Phy/Interface		\
        $(BspRoot)/PLM/Source/Common/MC1324xDrv \
	$(BspRoot)/PLM/Source/NVIC

include post.mak
