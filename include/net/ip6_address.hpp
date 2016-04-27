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
 *   This file includes definitions for IPv6 addresses.
 */

#ifndef IP6_ADDRESS_HPP_
#define IP6_ADDRESS_HPP_

#include <stdint.h>

#include <openthread.h>
#include <common/thread_error.hpp>

namespace Thread {
namespace Ip6 {

/**
 * @addtogroup core-ip6-ip6
 *
 * @{
 *
 */

/**
 * This class implements an IPv6 address object.
 *
 */
class Address: public otIp6Address
{
public:
    /**
     * IPv6 Address Scopes
     */
    enum
    {
        kNodeLocalScope      = 0,  ///< Node-Local scope
        kInterfaceLocalScope = 1,  ///< Interface-Local scope
        kLinkLocalScope      = 2,  ///< Link-Local scope
        kRealmLocalScope     = 3,  ///< Realm-Local scope
        kAdminLocalScope     = 4,  ///< Admin-Local scope
        kSiteLocalScope      = 5,  ///< Site-Local scope
        kOrgLocalScope       = 8,  ///< Organization-Local scope
        kGlobalScope         = 14, ///< Global scope
    };

    /**
     * This method indicates whether or not the IPv6 address is the Unspecified Address.
     *
     * @retval TRUE   If the IPv6 address is the Unspecified Address.
     * @retval FALSE  If the IPv6 address is not the Unspecified Address.
     *
     */
    bool IsUnspecified(void) const;

    /**
     * This method indicates whether or not the IPv6 address is the Loopback Address.
     *
     * @retval TRUE   If the IPv6 address is the Loopback Address.
     * @retval FALSE  If the IPv6 address is not the Loopback Address.
     *
     */
    bool IsLoopback(void) const;

    /**
     * This method indicates whether or not the IPv6 address scope is Interafce-Local.
     *
     * @retval TRUE   If the IPv6 address scope is Interface-Local.
     * @retval FALSE  If the IPv6 address scope is not Interface-Local.
     *
     */
    bool IsInterfaceLocal(void) const;

    /**
     * This method indicates whether or not the IPv6 address scope is Link-Local.
     *
     * @retval TRUE   If the IPv6 address scope is Link-Local.
     * @retval FALSE  If the IPv6 address scope is not Link-Local.
     *
     */
    bool IsLinkLocal(void) const;

    /**
     * This method indicates whether or not the IPv6 address is multicast address.
     *
     * @retval TRUE   If the IPv6 address is a multicast address.
     * @retval FALSE  If the IPv6 address scope is not a multicast address.
     *
     */
    bool IsMulticast(void) const;

    /**
     * This method indicates whether or not the IPv6 address is a link-local multicast address.
     *
     * @retval TRUE   If the IPv6 address is a link-local multicast address.
     * @retval FALSE  If the IPv6 address scope is not a link-local multicast address.
     *
     */
    bool IsLinkLocalMulticast(void) const;

    /**
     * This method indicates whether or not the IPv6 address is a link-local all nodes multicast address.
     *
     * @retval TRUE   If the IPv6 address is a link-local all nodes multicast address.
     * @retval FALSE  If the IPv6 address scope is not a link-local all nodes multicast address.
     *
     */
    bool IsLinkLocalAllNodesMulticast(void) const;

    /**
     * This method indicates whether or not the IPv6 address is a link-local all routers multicast address.
     *
     * @retval TRUE   If the IPv6 address is a link-local all routers multicast address.
     * @retval FALSE  If the IPv6 address scope is not a link-local all routers multicast address.
     *
     */
    bool IsLinkLocalAllRoutersMulticast(void) const;

    /**
     * This method indicates whether or not the IPv6 address is a realm-local multicast address.
     *
     * @retval TRUE   If the IPv6 address is a realm-local multicast address.
     * @retval FALSE  If the IPv6 address scope is not a realm-local multicast address.
     *
     */
    bool IsRealmLocalMulticast(void) const;

    /**
     * This method indicates whether or not the IPv6 address is a realm-local all nodes multicast address.
     *
     * @retval TRUE   If the IPv6 address is a realm-local all nodes multicast address.
     * @retval FALSE  If the IPv6 address scope is not a realm-local all nodes multicast address.
     *
     */
    bool IsRealmLocalAllNodesMulticast(void) const;

    /**
     * This method indicates whether or not the IPv6 address is a realm-local all routers multicast address.
     *
     * @retval TRUE   If the IPv6 address is a realm-local all routers multicast address.
     * @retval FALSE  If the IPv6 address scope is not a realm-local all routers multicast address.
     *
     */
    bool IsRealmLocalAllRoutersMulticast(void) const;

    /**
     * This method returns the IPv6 address scope.
     *
     * @returns The IPv6 address scope.
     *
     */
    uint8_t GetScope(void) const;

    /**
     * This method returns the number of IPv6 prefix bits that match.
     *
     * @param[in]  aOther  The IPv6 address to match against.
     *
     * @returns The number of IPv6 prefix bits that match.
     *
     */
    uint8_t PrefixMatch(const Address &aOther) const;

    /**
     * This method evaluates whether or not the IPv6 addresses match.
     *
     * @param[in]  aOther  The IPv6 address to compare.
     *
     * @retval TRUE   If the IPv6 addresses match.
     * @retval FALSE  If the IPv6 addresses do not match.
     *
     */
    bool operator==(const Address &aOther) const;

    /**
     * This method evaluates whether or not the IPv6 addresses differ.
     *
     * @param[in]  aOther  The IPv6 address to compare.
     *
     * @retval TRUE   If the IPv6 addresses differ.
     * @retval FALSE  If the IPv6 addresses do not differ.
     *
     */
    bool operator!=(const Address &aOther) const;

    /**
     * This method converts an IPv6 address string to binary.
     *
     * @param[in]  aBuf  A pointer to the NULL-terminated string.
     *
     * @retval kThreadError_None         Successfully parsed the IPv6 address string.
     * @retval kTheradError_InvalidArgs  Failed to parse the IPv6 address string.
     *
     */
    ThreadError FromString(const char *aBuf);

    /**
     * This method converts an IPv6 address object to a NULL-terminated string.
     *
     * @param[out]  aBuf   A pointer to the buffer.
     * @param[in]   aSize  The maximum size of the buffer.
     *
     * @returns A pointer to the buffer.
     *
     */
    const char *ToString(char *aBuf, uint16_t aSize) const;
};

/**
 * @}
 *
 */

}  // namespace Ip6
}  // namespace Thread

#endif  // NET_IP6_ADDRESS_HPP_
