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

#include <net/ip6_routes.hpp>
#include <net/netif.hpp>
#include <common/code_utils.hpp>
#include <common/message.hpp>

namespace Thread {

static Ip6Route *sRoutes = NULL;

ThreadError Ip6Routes::Add(Ip6Route &route)
{
    ThreadError error = kThreadError_None;

    for (Ip6Route *cur = sRoutes; cur; cur = cur->next)
    {
        VerifyOrExit(cur != &route, error = kThreadError_Busy);
    }

    route.next = sRoutes;
    sRoutes = &route;

exit:
    return error;
}

ThreadError Ip6Routes::Remove(Ip6Route &route)
{
    if (&route == sRoutes)
    {
        sRoutes = route.next;
    }
    else
    {
        for (Ip6Route *cur = sRoutes; cur; cur = cur->next)
        {
            if (cur->next == &route)
            {
                cur->next = route.next;
                break;
            }
        }
    }

    route.next = NULL;

    return kThreadError_None;
}

int Ip6Routes::Lookup(const Ip6Address &source, const Ip6Address &destination)
{
    int maxPrefixMatch = -1;
    uint8_t prefixMatch;
    int rval = -1;

    for (Ip6Route *cur = sRoutes; cur; cur = cur->next)
    {
        prefixMatch = cur->prefix.PrefixMatch(destination);

        if (prefixMatch < cur->prefixLength)
        {
            continue;
        }

        if (prefixMatch > cur->prefixLength)
        {
            prefixMatch = cur->prefixLength;
        }

        if (maxPrefixMatch > prefixMatch)
        {
            continue;
        }

        maxPrefixMatch = prefixMatch;
        rval = cur->interfaceId;
    }

    for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext())
    {
        if (netif->RouteLookup(source, destination, &prefixMatch) == kThreadError_None &&
            prefixMatch > maxPrefixMatch)
        {
            maxPrefixMatch = prefixMatch;
            rval = netif->GetInterfaceId();
        }
    }

    return rval;
}

}  // namespace Thread
