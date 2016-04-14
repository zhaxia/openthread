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

#include <cli/cli_ifconfig.hpp>
#include <common/code_utils.hpp>
#include <net/ip6_address.hpp>
#include <net/netif.hpp>

namespace Thread {
namespace Cli {

static const char kName[] = "ifconfig";

Ifconfig::Ifconfig(Server &server):
    Command(server)
{
}

const char *Ifconfig::GetName()
{
    return kName;
}

int PrintUsage(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = cur + buf_length;

    snprintf(cur, end - cur, "usage: ifconfig\r\n");
    cur += strlen(cur);

    return cur - buf;
}

int PrintStatus(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = buf + buf_length;

    for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext())
    {
        snprintf(cur, end - cur, "%s:\r\n", netif->GetName());
        cur += strlen(cur);

        for (const NetifUnicastAddress *addr = netif->GetUnicastAddresses(); addr; addr = addr->GetNext())
        {
            snprintf(cur, end - cur,  "  inet6 ");
            cur += strlen(cur);

            addr->address.ToString(cur, end - cur);
            cur += strlen(cur);

            snprintf(cur, end - cur, "/%d\r\n", addr->prefix_length);
            cur += strlen(cur);
        }
    }

    return cur - buf;
}

void Ifconfig::Run(int argc, char *argv[], Server &server)
{
    ThreadError error = kThreadError_None;
    char buf[512];
    char *cur = buf;
    char *end = cur + sizeof(buf);

    VerifyOrExit(argc == 0, error = kThreadError_InvalidArgs);

    cur += PrintStatus(buf, sizeof(buf));

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
