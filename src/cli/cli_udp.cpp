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

ThreadError Udp::Start()
{
    ThreadError error;

    otSockAddr sockaddr = {};
    sockaddr.mPort = 7335;

    SuccessOrExit(error = otOpenUdp6Socket(&mSocket, &HandleUdpReceive, this));
    SuccessOrExit(error = otBindUdp6Socket(&mSocket, &sockaddr));

exit:
    return error;
}

void Udp::HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo)
{
    Udp *obj = reinterpret_cast<Udp *>(context);
    obj->HandleUdpReceive(message, messageInfo);
}

void Udp::HandleUdpReceive(otMessage message, const otMessageInfo *messageInfo)
{
    ThreadError error = kThreadError_None;
    uint16_t payloadLength = otGetMessageLength(message) - otGetMessageOffset(message);
    otMessage reply;
    char buf[512];
    char *cmd;
    char *last;

    VerifyOrExit(payloadLength <= sizeof(buf), ;);
    otReadMessage(message, otGetMessageOffset(message), buf, payloadLength);

    if (buf[payloadLength - 1] == '\n')
    {
        buf[--payloadLength] = '\0';
    }

    if (buf[payloadLength - 1] == '\r')
    {
        buf[--payloadLength] = '\0';
    }

    VerifyOrExit((cmd = strtok_r(buf, " ", &last)) != NULL, ;);

    mPeer = *messageInfo;

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

        VerifyOrExit((reply = otNewUdp6Message()) != NULL, error = kThreadError_NoBufs);
        otAppendMessage(reply, buf, cur - buf);

        SuccessOrExit(error = otSendUdp6(&mSocket, reply, &mPeer));
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
        otFreeMessage(reply);
    }
}

ThreadError Udp::Output(const char *buf, uint16_t bufLength)
{
    ThreadError error = kThreadError_None;
    otMessage message;

    VerifyOrExit((message = otNewUdp6Message()) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = otSetMessageLength(message, bufLength));
    otWriteMessage(message, 0, buf, bufLength);
    SuccessOrExit(error = otSendUdp6(&mSocket, message, &mPeer));

exit:

    if (error != kThreadError_None && message != NULL)
    {
        otFreeMessage(message);
    }

    return error;
}

}  // namespace Cli
}  // namespace Thread
