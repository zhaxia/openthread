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

ifeq ($(BUILD_FEATURE_NLTHREAD),1)

include src/thread.mak

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

ARCHIVES = nlthread

IncludeFiles                                  := \
    $(COREINCLUDES)                              \

nlthread_SOURCES                               = \
    $(COREFILES)                                 \

ifeq ($(BUILD_FEATURE_COMMSIM),1)

VPATH                                         += \
    src/platform/commsim                         \

nlthread_SOURCES                              += \
    $(COREFILES_VNCP)                            \

nlthread_SOURCES                              += \
    platform/commsim/alarm.cc                    \
    platform/commsim/atomic.cc                   \
    platform/commsim/phy.cc                      \
    platform/commsim/sleep.cc                    \
    platform/commsim/uart.cc                     \

endif

nlthread_INCLUDES =                              \
    include                                      \
    src                                          \
    $(IncludeFiles)                              \
    $(NlThreadOptsPath)                          \
    $(NlThreadIncludePaths)                      \
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

nlthread_DEFINES =                               \

#ProductIncludeFiles     := $(ProductFpsDir)/nlthread/products/$(BuildProduct)/nlthread_opts.h
#ProductIncludePath      := $(BuildRoot)/$(ProductFpsDir)/nlthread/products/$(BuildProduct)/nlthread_opts.h
#ProductIncludeDirName   := ../../
#ProductResultIncDir     := $(call GenerateResultPaths,,$(ProductIncludeDirName))
#ProductResultIncPaths   := $(call GenerateResultPaths,,$(addprefix $(ProductIncludeDirName)/,$(ProductIncludeFiles)))
#NlThreadOptsPath        := $(ProductResultIncDir)/$(ProductFpsDir)/nlthread/products/$(BuildProduct)
#CleanPaths              += $(ProductResultIncPaths)
#TARGETS                 += $(ProductResultIncPaths)

#$(ProductResultIncPaths): $(ProductIncludePath)
#	$(install-result)
#prepare: $(ProductResultIncPaths)

IncludeDirName          := include
ResultIncDir            := $(call GenerateResultPaths,,$(IncludeDirName))
ResultIncPaths          := $(call GenerateResultPaths,$(NlThreadDir)/$(IncludeDirName),$(IncludeFiles))
CleanPaths              += $(ResultIncPaths)
TARGETS                 += $(ResultIncPaths)
$(ResultIncPaths): $(ResultIncDir)/%: %

$(ResultIncPaths): $(ResultIncDir)/%: %
	$(install-result)

endif # ifeq ($(BUILD_FEATURE_NLTHREAD),1)

include post.mak

