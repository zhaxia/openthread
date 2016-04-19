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

#ifndef IP6_ADDRESS_HPP_
#define IP6_ADDRESS_HPP_

#include <stdint.h>

#include <openthread.h>
#include <common/thread_error.hpp>

namespace Thread {

class Ip6Address: public otIp6Address
{
public:
    enum
    {
        kNodeLocalScope = 0,
        kInterfaceLocalScope = 1,
        kLinkLocalScope = 2,
        kRealmLocalScope = 3,
        kSiteLocalScope = 5,
        kOrgLocalScoep = 8,
        kGlobalScope = 14,
    };

    bool IsUnspecified() const;
    bool IsLoopback() const;
    bool IsInterfaceLocal() const;
    bool IsLinkLocal() const;

    bool IsMulticast() const;
    bool IsLinkLocalMulticast() const;
    bool IsLinkLocalAllNodesMulticast() const;
    bool IsLinkLocalAllRoutersMulticast() const;
    bool IsRealmLocalMulticast() const;
    bool IsRealmLocalAllNodesMulticast() const;
    bool IsRealmLocalAllRoutersMulticast() const;

    uint8_t GetScope() const;
    uint8_t PrefixMatch(const Ip6Address &other) const;

    bool operator==(const Ip6Address &other) const;
    bool operator!=(const Ip6Address &other) const;

    ThreadError FromString(const char *buf);
    const char *ToString(char *buf, uint16_t size) const;
};

}  // namespace Thread

#endif  // NET_IP6_ADDRESS_HPP_
