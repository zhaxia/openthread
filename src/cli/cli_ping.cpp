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

#include <cli/cli_ping.hpp>
#include <common/code_utils.hpp>
#include <net/ip6_address.hpp>

namespace Thread {
namespace Cli {

static const char kName[] = "ping";

Ping::Ping(Server &server):
    Command(server),
    mIcmp6Echo(&HandleEchoResponse, this)
{
}

const char *Ping::GetName()
{
    return kName;
}

int Ping::PrintUsage(char *buf, uint16_t bufLength)
{
    char *cur = buf;
    char *end = cur + bufLength;

    snprintf(cur, end - cur, "usage: ping [-I interface] [-i wait] [-c count] [-s size] host\r\n");
    cur += strlen(cur);

    return cur - buf;
}

void Ping::EchoRequest()
{
    uint8_t buf[2048];

    for (int i = 0; i < mLength; i++)
    {
        buf[i] = i;
    }

    mIcmp6Echo.SendEchoRequest(mSockAddr, buf, mLength);
}

void Ping::Run(int argc, char *argv[], Server &server)
{
    char buf[128];
    char *cur = buf;
    char *end = cur + sizeof(buf);
    char *endptr;

    Netif *netif;

    mServer = &server;
    mLength = 0;
    memset(&mSockAddr, 0, sizeof(mSockAddr));

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
            mSockAddr.mScopeId = netif->GetInterfaceId();
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            VerifyOrExit(++i < argc, ;);
            mLength = strtol(argv[i], &endptr, 0);
            VerifyOrExit(*endptr == '\0', ;);
        }
        else
        {
            VerifyOrExit(mSockAddr.GetAddress().FromString(argv[i]) == kThreadError_None, ;);
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

void Ping::HandleEchoResponse(void *context, Message &message, const Ip6MessageInfo &messageInfo)
{
    Ping *obj = reinterpret_cast<Ping *>(context);
    obj->HandleEchoResponse(message, messageInfo);
}

void Ping::HandleEchoResponse(Message &message, const Ip6MessageInfo &messageInfo)
{
    char buf[256];
    char *cur = buf;
    char *end = cur + sizeof(buf);
    Icmp6Header icmp6Header;
    Netif *netif;

    message.Read(message.GetOffset(), sizeof(icmp6Header), &icmp6Header);

    snprintf(cur, end - cur, "%d bytes from ", message.GetLength() - message.GetOffset());
    cur += strlen(cur);

    messageInfo.GetPeerAddr().ToString(cur, end - cur);
    cur += strlen(cur);

    netif = Netif::GetNetifById(messageInfo.mInterfaceId);
    snprintf(cur, end - cur, "%%%s: icmp_seq=%d hlim=%d",
             netif->GetName(), icmp6Header.GetSequence(), messageInfo.mHopLimit);
    cur += strlen(cur);

    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    mServer->Output(buf, cur - buf);
}

}  // namespace Cli
}  // namespace Thread
