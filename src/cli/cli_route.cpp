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
 *   This file implements the CLI commands for configuring and managing IPv6 routes.
 */

#include <stdlib.h>

#include <cli/cli_route.hpp>
#include <common/code_utils.hpp>

namespace Thread {
namespace Cli {

static const char kName[] = "route";

Route::Route(Server &server):
    Command(server)
{
    memset(&mRoute, 0, sizeof(mRoute));
}

const char *Route::GetName()
{
    return kName;
}

int Route::PrintUsage(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

    snprintf(cur, end - cur,
             "usage: route\r\n"
             "  add <prefix>/<plen> <interface>\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int Route::AddRoute(int argc, char *argv[], char *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_InvalidArgs;
    char *cur = buf;
    char *end = cur + bufLength;
    int argcur = 0;

    char *prefixLength;
    char *endptr;

    if (argcur >= argc)
    {
        ExitNow();
    }


    if ((prefixLength = strchr(argv[argcur], '/')) == NULL)
    {
        ExitNow();
    }

    *prefixLength++ = '\0';

    if (mRoute.prefix.FromString(argv[argcur]) != kThreadError_None)
    {
        ExitNow();
    }

    mRoute.prefixLength = strtol(prefixLength, &endptr, 0);

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
            mRoute.interfaceId = netif->GetInterfaceId();
            Ip6Routes::Add(mRoute);
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
