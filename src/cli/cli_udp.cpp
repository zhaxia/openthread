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

#include <stdio.h>
#include <string.h>

#include <cli/cli_command.hpp>
#include <cli/cli_udp.hpp>
#include <common/code_utils.hpp>

namespace Thread {
namespace Cli {

Udp::Udp():
    mSocket(&HandleUdpReceive, this)
{
}

ThreadError Udp::Start()
{
    struct sockaddr_in6 sockaddr;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.mPort = 7335;

    return mSocket.Bind(&sockaddr);
}

void Udp::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &messageInfo)
{
    Udp *obj = reinterpret_cast<Udp *>(context);
    obj->HandleUdpReceive(message, messageInfo);
}

void Udp::HandleUdpReceive(Message &message, const Ip6MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    uint16_t payloadLength = message.GetLength() - message.GetOffset();
    Message *reply;
    char buf[512];
    char *cmd;
    char *last;

    VerifyOrExit(payloadLength <= sizeof(buf), ;);
    message.Read(message.GetOffset(), payloadLength, buf);

    if (buf[payloadLength - 1] == '\n')
    {
        buf[--payloadLength] = '\0';
    }

    if (buf[payloadLength - 1] == '\r')
    {
        buf[--payloadLength] = '\0';
    }

    VerifyOrExit((cmd = strtok_r(buf, " ", &last)) != NULL, ;);

    mPeer = messageInfo;

    if (strncmp(cmd, "?", 1) == 0)
    {
        char *cur = buf;
        char *end = cur + sizeof(buf);

        snprintf(cur, end - cur, "%s", "Commands:\r\n");
        cur += strlen(cur);

        for (Command *command = mCommands; command; command = command->GetNext())
        {
            snprintf(cur, end - cur, "%s\r\n", command->GetName());
            cur += strlen(cur);
        }

        VerifyOrExit((reply = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
        reply->Append(buf, cur - buf);

        SuccessOrExit(error = mSocket.SendTo(*reply, mPeer));
    }
    else
    {
        int argc;
        char *argv[kMaxArgs];

        for (argc = 0; argc < kMaxArgs; argc++)
        {
            if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL)
            {
                break;
            }
        }

        for (Command *command = mCommands; command; command = command->GetNext())
        {
            if (strcmp(cmd, command->GetName()) == 0)
            {
                command->Run(argc, argv, *this);
                break;
            }
        }
    }

exit:

    if (error != kThreadError_None && reply != NULL)
    {
        Message::Free(*reply);
    }
}

ThreadError Udp::Output(const char *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(bufLength));
    message->Write(0, bufLength, buf);
    SuccessOrExit(error = mSocket.SendTo(*message, mPeer));

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

}  // namespace Cli
}  // namespace Thread
