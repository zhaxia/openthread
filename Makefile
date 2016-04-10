#
#    Copyright (c) 2016 Nest Labs, Inc.
#    All rights reserved.
#
#    This document is the property of Nest Labs. It is considered
#    confidential and proprietary information.
#
#    This document may not be reproduced or transmitted in any form,
#    in whole or in part, without the express written permission of
#    Nest Labs.
#
#    Description:
#      This is a make file for OpenThread library build out of Nest's repo.
#

include pre.mak

ifeq ($(BUILD_FEATURE_OPENTHREAD),1)

include $(OpenThreadSrcPath)/openthread.mak

.DEFAULT_GOAL = all

ARCHIVES = openthread

VPATH                                                              = \
    $(OpenThreadSrcPath)                                             \
    $(OpenThreadSrcPath)/include                                     \
    $(OpenThreadSrcPath)/lib                                         \
    $(OpenThreadSrcPath)/src                                         \
    $(OpenThreadSrcPath)/src/coap                                    \
    $(OpenThreadSrcPath)/src/common                                  \
    $(OpenThreadSrcPath)/src/crypto                                  \
    $(OpenThreadSrcPath)/src/mac                                     \
    $(OpenThreadSrcPath)/src/meshcop                                 \
    $(OpenThreadSrcPath)/src/ncp                                     \
    $(OpenThreadSrcPath)/src/net                                     \
    $(OpenThreadSrcPath)/src/thread                                  \
    $(OpenThreadSrcPath)/third_party                                 \

openthread_SOURCES                                                 = \
    $(OPENTHREAD_SOURCES)                                            \

openthread_INCLUDES =                                                \
    $(OpenThreadIncludePaths)                                        \
    $(OpenThreadSrcPath)/lib                                         \
    $(OpenThreadSrcPath)/src                                         \
    $(OpenThreadSrcPath)/third_party                                 \
#    $(NLERIncludePaths)                                             \
#    $(LwIPIncludePaths)                                             \
#    $(NlLwipIncludePath)                                            \
#    $(NlPlatformIncludePaths)                                       \
#    $(FreeRTOSIncludePaths)                                         \
#    $(NlAssertIncludePaths)                                         \
#    $(NlUtilitiesIncludePath)                                       \
#    $(NlSystemIncludePath)                                          \

### Platform Specific Source Files

VPATH                                                             += \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform                \

openthread_SOURCES                                                += \
    $(OPENTHREAD_SOURCES_VNCP)                                       \

openthread_SOURCES                                                += \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/alarm.cc       \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/atomic.cc      \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/phy.cc         \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/sleep.cc       \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/uart.cc        \

openthread_DEFINES =                                                 \

endif # ifeq ($(BUILD_FEATURE_OPENTHREAD),1)

include post.mak

