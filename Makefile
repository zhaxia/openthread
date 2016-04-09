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

include openthread.mak

.DEFAULT_GOAL = all

VPATH                                          = \
    include                                      \
    include/common                               \
    lib                                          \
    src                                          \
    src/coap                                     \
    src/common                                   \
    src/crypto                                   \
    src/mac                                      \
    src/net                                      \
    src/ncp                                      \
    src/platform                                 \
    src/platform/common                          \
    src/protobuf                                 \
    src/thread                                   \
    third_party                                  \

ARCHIVES = openthread

IncludeFiles                                  := \
    $(OPENTHREAD_INCLUDES)                       \

openthread_SOURCES                             = \
    $(OPENTHREAD_SOURCES)                        \

ifeq ($(BUILD_FEATURE_COMMSIM),1)

VPATH                                         += \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform    \

openthread_SOURCES                            += \
    $(OPENTHREAD_SOURCES_VNCP)                   \

openthread_SOURCES                            += \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/alarm.cc     \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/alarm.cc                    \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/atomic.cc                   \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/phy.cc                      \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/sleep.cc                    \
    $(BuildRoot)/$(ProductFpsDir)/openthread/platform/uart.cc                     \

endif

openthread_INCLUDES =                            \
    $(OpenThreadIncludePaths)                    \
    $(OpenThreadSrcPath)/lib                     \
    $(OpenThreadSrcPath)/src                     \
    $(OpenThreadSrcPath)/third_party             \
#    $(AppsIncludePaths)                          \
#    $(NLERIncludePaths)                          \
#    $(LwIPIncludePaths)                          \
#    $(WicedIncludePaths)                         \
#    $(NlLwipIncludePath)                         \
#    $(NlPlatformIncludePaths)                    \
#    $(FreeRTOSIncludePaths)                      \
#    $(NlIoIncludePaths)                          \
#    $(NlAssertIncludePaths)                      \
#    $(NlUtilitiesIncludePath)                    \
#    $(NlSystemIncludePath)                       \
#    $(NlEnvIncludePaths)                         \
#    $(NlNetworkManagerIncludePaths)              \

openthread_DEFINES =                             \

IncludeDirName          := include
ResultIncDir            := $(call GenerateResultPaths,,$(IncludeDirName))
ResultIncPaths          := $(call GenerateResultPaths,$(OpenThreadDir)/$(IncludeDirName),$(IncludeFiles))
CleanPaths              += $(ResultIncPaths)
TARGETS                 += $(ResultIncPaths)
$(ResultIncPaths): $(ResultIncDir)/%: %

$(ResultIncPaths): $(ResultIncDir)/%: %
	$(install-result)

endif # ifeq ($(BUILD_FEATURE_OPENTHREAD),1)

include post.mak

