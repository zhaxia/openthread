#
#    Copyright 2016 Nest Labs Inc. All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

include $(abs_top_nlbuild_autotools_dir)/automake/pre.am

# Always package (e.g. for 'make dist') these subdirectories.

DIST_SUBDIRS                                                       = \
    scripts                                                          \
    src                                                              \
    $(NULL)

# Always build (e.g. for 'make all') these subdirectories.

SUBDIRS                                                            = \
    src                                                              \
    $(NULL)

all:

if OPENTHREAD_BUILD_TESTS

# First, list all essential program and script tests that MUST be run.

check_PROGRAMS =

check_SCRIPTS                                                      = \
    scripts/Cert_5_1_01_RouterAttach.py                              \
    scripts/Cert_5_1_02_ChildAddressTimeout.py                       \
    scripts/Cert_5_1_03_RouterAddressReallocation.py                 \
    scripts/Cert_5_1_06_RemoveRouterId.py                            \
    scripts/Cert_5_1_08_RouterAttachConnectivity.py                  \
    scripts/Cert_5_1_09_REEDAttachConnectivity.py                    \
    scripts/Cert_5_1_10_RouterAttachLinkQuality.py                   \
    scripts/Cert_5_1_11_REEDAttachLinkQuality.py                     \
    scripts/Cert_5_1_12_NewRouterNeighborSync.py                     \
    scripts/Cert_5_1_13_RouterReset.py                               \
    scripts/Cert_5_2_01_BecomeActiveRouter.py                        \
    scripts/Cert_5_2_05_AddressQuery.py                              \
    scripts/Cert_5_2_07_REEDSynchronization.py                       \
    scripts/Cert_5_3_01_LinkLocal.py                                 \
    scripts/Cert_5_3_07_DuplicateAddress.py                          \
    scripts/Cert_5_3_08_ChildAddressSet.py                           \
    scripts/Cert_5_5_01_LeaderReset.py                               \
    scripts/Cert_5_8_01_KeySynchronization.py                        \
    scripts/Cert_5_8_02_KeyIncrement.py                              \
    scripts/Cert_5_8_03_KeyIncrementRollOver.py                      \
    $(NULL)

# List all other non-essential program and script tests that MAY be run.

if !OPENTHREAD_WITH_MINIMUM_SET_OF_TESTS

check_SCRIPTS                                                     += \
    scripts/Cert_5_1_07_MaxChildCount.py                             \
    scripts/Cert_5_2_02_LeaderReject1Hop.py                          \
    scripts/Cert_5_2_03_LeaderReject2Hops.py                         \
    scripts/Cert_5_2_04_REEDUpgrade.py                               \
    scripts/Cert_5_3_02_RealmLocal.py                                \
    scripts/Cert_5_3_03_AddressQuery.py                              \
    scripts/Cert_5_3_04_AddressMapCache.py                           \
    scripts/Cert_5_3_05_RoutingLinkQuality.py                        \
    scripts/Cert_5_3_06_RouterIdMask.py                              \
    scripts/Cert_5_3_10_AddressQuery.py                              \
    $(NULL)

#  Failing
#    scripts/Cert_5_1_04_RouterAddressReallocation.py                 \
#    scripts/Cert_5_1_05_RouterAddressTimeout.py                      \
#    $(NULL)

endif # OPENTHREAD_WITH_ESSENTIAL_TESTS_ONLY

TESTS_ENVIRONMENT                                                  = \
    export                                                           \
    abs_builddir='$(abs_builddir)'                                   \
    abs_srcdir='$(abs_srcdir)'                                       \
    VERBOSE=1;                                                       \
    $(NULL)

TESTS                                                              = \
    $(check_PROGRAMS)                                                \
    $(check_SCRIPTS)                                                 \
    $(NULL)

endif # OPENTHREAD_BUILD_TESTS

include $(abs_top_nlbuild_autotools_dir)/automake/post.am
