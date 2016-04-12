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

#include <cli/cli_ping.h>
#include <common/code_utils.h>
#include <net/ip6_address.h>

namespace Thread {
namespace Cli {

static const char kName[] = "ping";

Ping::Ping(Server &server):
    Command(server),
    m_icmp6_echo(&HandleEchoResponse, this)
{
}

const char *Ping::GetName()
{
    return kName;
}

int Ping::PrintUsage(char *buf, uint16_t buf_length)
{
    char *cur = buf;
    char *end = cur + buf_length;

    snprintf(cur, end - cur, "usage: ping [-I interface] [-i wait] [-c count] [-s size] host\r\n");
    cur += strlen(cur);

    return cur - buf;
}

void Ping::EchoRequest()
{
    uint8_t buf[2048];

    for (int i = 0; i < m_length; i++)
    {
        buf[i] = i;
    }

    m_icmp6_echo.SendEchoRequest(m_sockaddr, buf, m_length);
}

void Ping::Run(int argc, char *argv[], Server &server)
{
    char buf[128];
    char *cur = buf;
    char *end = cur + sizeof(buf);
    char *endptr;

    Netif *netif;

    m_server = &server;
    m_length = 0;
    memset(&m_sockaddr, 0, sizeof(m_sockaddr));

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            ExitNow();
        }
        else if (strcmp(argv[i], "-I") == 0)
        {
            VerifyOrExit(++i < argc, ;);
            VerifyOrExit((netif = Netif::GetNetifByName(argv[i])) != NULL, ;);
            m_sockaddr.sin6_scope_id = netif->GetInterfaceId();
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            VerifyOrExit(++i < argc, ;);
            m_length = strtol(argv[i], &endptr, 0);
            VerifyOrExit(*endptr == '\0', ;);
        }
        else
        {
            VerifyOrExit(m_sockaddr.sin6_addr.FromString(argv[i]) == kThreadError_None, ;);
            EchoRequest();
            return;
        }
    }

exit:
    cur += PrintUsage(cur, end - cur);
    snprintf(cur, end - cur, "Done\r\n");
    cur += strlen(cur);
    server.Output(buf, cur - buf);
}

void Ping::HandleEchoResponse(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    Ping *obj = reinterpret_cast<Ping *>(context);
    obj->HandleEchoResponse(message, message_info);
}

void Ping::HandleEchoResponse(Message &message, const Ip6MessageInfo &message_info)
{
    char buf[256];
    char *cur = buf;
    char *end = cur + sizeof(buf);
    Icmp6Header icmp6_header;
    Netif *netif;

    message.Read(message.GetOffset(), sizeof(icmp6_header), &icmp6_header);

    snprintf(cur, end - cur, "%d bytes from ", message.GetLength() - message.GetOffset());
    cur += strlen(cur);

    message_info.peer_addr.ToString(cur, end - cur);
    cur += strlen(cur);

    netif = Netif::GetNetifById(message_info.interface_id);
    snprintf(cur, end - cur, "%%%s: icmp_seq=%d hlim=%d",
             netif->GetName(), icmp6_header.GetSequence(), message_info.hop_limit);
    cur += strlen(cur);

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    m_server->Output(buf, cur - buf);
}

}  // namespace Cli
}  // namespace Thread
