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
    src                                          \
    src/app                                      \
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
    src/tun                                      \

ARCHIVES = openthread

IncludeFiles                                  := \
    $(OPENTHREAD_INCLUDES)                       \

openthread_SOURCES                             = \
    $(OPENTHREAD_SOURCES)                        \

ifeq ($(BUILD_FEATURE_COMMSIM),1)

VPATH                                         += \
    src/platform/commsim                         \

openthread_SOURCES                            += \
    $(OPENTHREAD_SOURCES_VNCP)                   \

openthread_SOURCES                            += \
    platform/commsim/alarm.cc                    \
    platform/commsim/atomic.cc                   \
    platform/commsim/phy.cc                      \
    platform/commsim/sleep.cc                    \
    platform/commsim/uart.cc                     \

endif

openthread_INCLUDES =                            \
    include                                      \
    src                                          \
    $(IncludeFiles)                              \
    $(OpenThreadOptsPath)                        \
    $(OpenThreadIncludePaths)                    \
    $(AppsIncludePaths)                          \
    $(NLERIncludePaths)                          \
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

#ProductIncludeFiles     := $(ProductFpsDir)/openthread/products/$(BuildProduct)/openthread_opts.h
#ProductIncludePath      := $(BuildRoot)/$(ProductFpsDir)/openthread/products/$(BuildProduct)/openthread_opts.h
#ProductIncludeDirName   := ../../
#ProductResultIncDir     := $(call GenerateResultPaths,,$(ProductIncludeDirName))
#ProductResultIncPaths   := $(call GenerateResultPaths,,$(addprefix $(ProductIncludeDirName)/,$(ProductIncludeFiles)))
#OpenThreadOptsPath      := $(ProductResultIncDir)/$(ProductFpsDir)/openthread/products/$(BuildProduct)
#CleanPaths              += $(ProductResultIncPaths)
#TARGETS                 += $(ProductResultIncPaths)

#$(ProductResultIncPaths): $(ProductIncludePath)
#	$(install-result)
#prepare: $(ProductResultIncPaths)

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

