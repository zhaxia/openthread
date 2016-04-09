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

#include <cli/cli_route.h>
#include <common/code_utils.h>
#include <stdlib.h>

namespace Thread {
namespace Cli {

static const char kName[] = "route";

Route::Route(Server &server):
    Command(server)
{
    memset(&m_route, 0, sizeof(m_route));
}

const char *Route::GetName()
{
    return kName;
}

int Route::PrintUsage(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = cur + buf_length;

    snprintf(cur, end - cur,
             "usage: route\r\n"
             "  add <prefix>/<plen> <interface>\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Route::AddRoute(int argc, char *argv[], char *buf, uint16_t buf_length)
{
    ThreadError error = kThreadError_InvalidArgs;
    char *cur = buf;
    char *end = cur + buf_length;
    int argcur = 0;

    char *prefix_length;
    char *endptr;

    if (argcur >= argc)
    {
        ExitNow();
    }


    if ((prefix_length = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefix_length++ = '\0';

    if (m_route.prefix.FromString(argv[argcur]) != kThreadError_None)
    {
        ExitNow();
    }

    m_route.prefix_length = strtol(prefix_length, &endptr, 0);

    if (*endptr != '\0')
    {
        ExitNow();
    }

    if (++argcur >= argc)
    {
        ExitNow();
    }

    for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext())
    {
        if (strcmp(netif->GetName(), argv[argcur]) == 0)
        {
            m_route.interface_id = netif->GetInterfaceId();
            Ip6Routes::Add(m_route);
            ExitNow(error = kThreadError_None);
        }
    }

exit:

    if (error != kThreadError_None)
    {
        cur += PrintUsage(cur, end - cur);
    }

    return cur - buf;
}

void Route::Run(int argc, char *argv[], Server &server)
{
    ThreadError error = kThreadError_InvalidArgs;
    char buf[512];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            ExitNow();
        }
        else if (strcmp(argv[i], "add") == 0)
        {
            i++;
            cur += AddRoute(argc - i, &argv[i], cur, end - cur);
            ExitNow(error = kThreadError_None);
        }
    }

exit:

    if (error != kThreadError_None)
    {
        cur += PrintUsage(cur, end - cur);
    }

    snprintf(cur, end - cur, "Done\r\n");
    cur += strlen(cur);
    server.Output(buf, cur - buf);
}

}  // namespace Cli
}  // namespace Thread
