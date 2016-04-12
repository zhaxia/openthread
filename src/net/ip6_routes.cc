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

#include <net/ip6_routes.h>
#include <net/netif.h>
#include <common/code_utils.h>
#include <common/message.h>

namespace Thread {

static Ip6Route *s_routes = NULL;

ThreadError Ip6Routes::Add(Ip6Route &route)
{
    ThreadError error = kThreadError_None;

    for (Ip6Route *cur = s_routes; cur; cur = cur->next)
    {
        VerifyOrExit(cur != &route, error = kThreadError_Busy);
    }

    route.next = s_routes;
    s_routes = &route;

exit:
    return error;
}

ThreadError Ip6Routes::Remove(Ip6Route &route)
{
    if (&route == s_routes)
    {
        s_routes = route.next;
    }
    else
    {
        for (Ip6Route *cur = s_routes; cur; cur = cur->next)
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
    int max_prefix_match = -1;
    uint8_t prefix_match;
    int rval = -1;

    for (Ip6Route *cur = s_routes; cur; cur = cur->next)
    {
        prefix_match = cur->prefix.PrefixMatch(destination);

        if (prefix_match < cur->prefix_length)
        {
            continue;
        }

        if (prefix_match > cur->prefix_length)
        {
            prefix_match = cur->prefix_length;
        }

        if (max_prefix_match > prefix_match)
        {
            continue;
        }

        max_prefix_match = prefix_match;
        rval = cur->interface_id;
    }

    for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext())
    {
        if (netif->RouteLookup(source, destination, &prefix_match) == kThreadError_None &&
            prefix_match > max_prefix_match)
        {
            max_prefix_match = prefix_match;
            rval = netif->GetInterfaceId();
        }
    }

    return rval;
}

}  // namespace Thread
