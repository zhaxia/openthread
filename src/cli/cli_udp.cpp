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
 *   This file implements the CLI server on a UDP socket.
 */

#include <stdio.h>
#include <string.h>

#include <cli/cli.hpp>
#include <cli/cli_udp.hpp>
#include <common/code_utils.hpp>

namespace Thread {
namespace Cli {

ThreadError Udp::Start(void)
{
    ThreadError error;

    otSockAddr sockaddr = {};
    sockaddr.mPort = 7335;

    SuccessOrExit(error = otOpenUdpSocket(&mSocket, &HandleUdpReceive, this));
    SuccessOrExit(error = otBindUdpSocket(&mSocket, &sockaddr));

exit:
    return error;
}

void Udp::HandleUdpReceive(void *aContext, otMessage aMessage, const otMessageInfo *aMessageInfo)
{
    Udp *obj = reinterpret_cast<Udp *>(aContext);
    obj->HandleUdpReceive(aMessage, aMessageInfo);
}

void Udp::HandleUdpReceive(otMessage aMessage, const otMessageInfo *aMessageInfo)
{
    ThreadError error = kThreadError_None;
    uint16_t payloadLength = otGetMessageLength(aMessage) - otGetMessageOffset(aMessage);
    char buf[512];

    VerifyOrExit(payloadLength <= sizeof(buf), ;);
    otReadMessage(aMessage, otGetMessageOffset(aMessage), buf, payloadLength);

    if (buf[payloadLength - 1] == '\n')
    {
        buf[--payloadLength] = '\0';
    }

    if (buf[payloadLength - 1] == '\r')
    {
        buf[--payloadLength] = '\0';
    }

    mPeer = *aMessageInfo;

    Interpreter::ProcessLine(buf, payloadLength, *this);

exit:
    {}
}

ThreadError Udp::Output(const char *aBuf, uint16_t aBufLength)
{
    ThreadError error = kThreadError_None;
    otMessage message;

    VerifyOrExit((message = otNewUdpMessage()) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = otSetMessageLength(message, aBufLength));
    otWriteMessage(message, 0, aBuf, aBufLength);
    SuccessOrExit(error = otSendUdp(&mSocket, message, &mPeer));

exit:

    if (error != kThreadError_None && message != NULL)
    {
        otFreeMessage(message);
    }

    return error;
}

}  // namespace Cli
}  // namespace Thread
