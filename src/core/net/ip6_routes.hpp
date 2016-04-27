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

#ifndef IP6_ROUTES_HPP_
#define IP6_ROUTES_HPP_

/**
 * @file
 *   This file includes definitions for manipulating IPv6 routing tables.
 */

#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/ip6_address.hpp>

namespace Thread {
namespace Ip6 {

/**
 * @addtogroup core-ip6-ip6
 *
 * @{
 *
 */

/**
 * This structure represents an IPv6 route.
 *
 */
struct Route
{
    Address       mPrefix;        ///< The IPv6 prefix.
    uint8_t       mPrefixLength;  ///< The IPv6 prefix length.
    uint8_t       mInterfaceId;   ///< The interface identifier.
    struct Route *mNext;          ///< A pointer to the next IPv6 route.
};

/**
 * This class implements IPv6 route management.
 *
 */
class Routes
{
public:
    /**
     * This static method adds an IPv6 route.
     *
     * @param[in]  aRoute  A reference to the IPv6 route.
     *
     * @retval kThreadError_None  Successfully added the route.
     * @retval kThreadError_Busy  The route was already added.
     *
     */
    static ThreadError Add(Route &aRoute);

    /**
     * This static method removes an IPv6 route.
     *
     * @param[in]  aRoute  A reference to the IPv6 route.
     *
     * @retval kThreadError_None         Successfully removed the route.
     * @retval kThreadError_InvalidArgs  The route was not added.
     *
     */
    static ThreadError Remove(Route &aRoute);

    /**
     * This static method performs source-destination route lookup.
     *
     * @param[in]  aSource       The IPv6 source address.
     * @param[in]  aDestination  The IPv6 destination address.
     *
     * @returns The interface identifier for the best route or -1 if no route is available.
     *
     */
    static int Lookup(const Address &aSource, const Address &aDestination);
};

/**
 * @}
 *
 */

}  // namespace Ip6
}  // namespace Thread

#endif  // NET_IP6_ROUTES_HPP_
