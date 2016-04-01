/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
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
