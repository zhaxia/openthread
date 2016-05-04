/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 * @file
 *   This file includes definitions for MLE functionality required by the Thread Child, Router, and Leader roles.
 */

#ifndef MLE_CONSTANTS_HPP_
#define MLE_CONSTANTS_HPP_

namespace Thread {
namespace Mle {

/**
 * @addtogroup core-mle-core
 *
 */

enum
{
    kMaxChildren                = OPENTHREAD_CONFIG_MAX_CHILDREN,
};

/**
 * MLE Protocol Constants
 *
 */
enum
{
    kVersion                    = 1,     ///< MLE Version
    kUdpPort                    = 19788, ///< MLE UDP Port
    kParentRequestRouterTimeout = 1000,  ///< Router Request timeout
    kParentRequestChildTimeout  = 2000,  ///< End Device Request timeout
};

enum
{
    kChildIdMask                = 0x1ff,
    kRouterIdOffset             = 10,
    kRlocPrefixLength           = 14,   ///< Prefix length of RLOC in bytes
};

/**
 * Routing Protocol Contstants
 *
 */
enum
{
    kAdvertiseIntervalMin       = 1,    ///< ADVERTISEMENT_I_MIN (seconds)
    kAdvertiseIntervalMax       = 32,   ///< ADVERTISEMENT_I_MAX (seconds)
    kRouterIdReuseDelay         = 100,  ///< ID_REUSE_DELAY (seconds)
    kRouterIdSequencePeriod     = 10,   ///< ID_SEQUENCE_PERIOD (seconds)
    kMaxNeighborAge             = 100,  ///< MAX_NEIGHBOR_AGE (seconds)
    kMaxRouteCost               = 16,   ///< MAX_ROUTE_COST
    kMaxRouterId                = 62,   ///< MAX_ROUTER_ID
    kMaxRouters                 = 32,   ///< MAX_ROUTERS
    kMinDowngradeNeighbors      = 7,    ///< MIN_DOWNGRADE_NEIGHBORS
    kNetworkIdTimeout           = 120,  ///< NETWORK_ID_TIMEOUT (seconds)
    kParentRouteToLeaderTimeout = 20,   ///< PARENT_ROUTE_TO_LEADER_TIMEOUT (seconds)
    kRouterSelectionJitter      = 120,  ///< ROUTER_SELECTION_JITTER (seconds)
    kRouterDowngradeThreshold   = 23,   ///< ROUTER_DOWNGRADE_THRESHOLD (routers)
    kRouterUpgradeThreadhold    = 16,   ///< ROUTER_UPGRADE_THRESHOLD (routers)
    kMaxLeaderToRouterTimeout   = 90,   ///< INFINITE_COST_TIMEOUT (seconds)
    kReedAdvertiseInterval      = 570,  ///< REED_ADVERTISEMENT_INTERVAL (seconds)
    kReedAdvertiseJitter        = 60,   ///< REED_ADVERTISEMENT_JITTER (seconds)
};


}  // namespace Mle

/**
 * @}
 *
 */

}  // namespace Thread

#endif  // MLE_CONSTANTS_HPP_
