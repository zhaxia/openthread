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

TARGET = thread_posix

ARCHIVES = $(TARGET)

# Define SOURCES, INCLUDES, DEFINES according to Nest make macro naming:
# <module>_SOURCES, where <module> is one of
# PROGRAMS, IMAGES, ARCHIVES, LIBRARIES

$(TARGET)_SOURCES = $(wildcard *.cc)

$(TARGET)_INCLUDES =	       \
	$(BuildRoot)/src/lib/thread/src

include post.mak
