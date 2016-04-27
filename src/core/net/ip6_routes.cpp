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
 *   This file implements IPv6 route tables.
 */

#include <net/ip6_routes.hpp>
#include <net/netif.hpp>
#include <common/code_utils.hpp>
#include <common/message.hpp>

namespace Thread {
namespace Ip6 {

static Route *sRoutes = NULL;

ThreadError Routes::Add(Route &aRoute)
{
    ThreadError error = kThreadError_None;

    for (Route *cur = sRoutes; cur; cur = cur->mNext)
    {
        VerifyOrExit(cur != &aRoute, error = kThreadError_Busy);
    }

    aRoute.mNext = sRoutes;
    sRoutes = &aRoute;

exit:
    return error;
}

ThreadError Routes::Remove(Route &aRoute)
{
    if (&aRoute == sRoutes)
    {
        sRoutes = aRoute.mNext;
    }
    else
    {
        for (Route *cur = sRoutes; cur; cur = cur->mNext)
        {
            if (cur->mNext == &aRoute)
            {
                cur->mNext = aRoute.mNext;
                break;
            }
        }
    }

    aRoute.mNext = NULL;

    return kThreadError_None;
}

int Routes::Lookup(const Address &aSource, const Address &aDestination)
{
    int maxPrefixMatch = -1;
    uint8_t prefixMatch;
    int rval = -1;

    for (Route *cur = sRoutes; cur; cur = cur->mNext)
    {
        prefixMatch = cur->mPrefix.PrefixMatch(aDestination);

        if (prefixMatch < cur->mPrefixLength)
        {
            continue;
        }

        if (prefixMatch > cur->mPrefixLength)
        {
            prefixMatch = cur->mPrefixLength;
        }

        if (maxPrefixMatch > prefixMatch)
        {
            continue;
        }

        maxPrefixMatch = prefixMatch;
        rval = cur->mInterfaceId;
    }

    for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext())
    {
        if (netif->RouteLookup(aSource, aDestination, &prefixMatch) == kThreadError_None &&
            prefixMatch > maxPrefixMatch)
        {
            maxPrefixMatch = prefixMatch;
            rval = netif->GetInterfaceId();
        }
    }

    return rval;
}

}  // namespace Ip6
}  // namespace Thread
