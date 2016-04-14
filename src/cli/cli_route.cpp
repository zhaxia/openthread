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

#include <stdlib.h>

#include <cli/cli_route.hpp>
#include <common/code_utils.hpp>

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
