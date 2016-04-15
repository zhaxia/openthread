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

#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/thread_error.hpp>
#include <net/ip6.hpp>
#include <net/udp6.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static Udp6Socket *sSockets = NULL;
static uint16_t sEphemeralPort = 49152;

Udp6Socket::Udp6Socket(ReceiveHandler handler, void *context)
{
    memset(&mSockName, 0, sizeof(mSockName));
    memset(&mPeerName, 0, sizeof(mPeerName));
    mHandler = handler;
    mContext = context;
    mNext = NULL;
}

ThreadError Udp6Socket::Bind(const struct sockaddr_in6 *sockaddr)
{
    ThreadError error = kThreadError_None;

    if (sockaddr)
    {
        mSockName = *sockaddr;
    }

    for (Udp6Socket *cur = sSockets; cur; cur = cur->mNext)
    {
        if (cur == this)
        {
            ExitNow();
        }
    }

    mNext = sSockets;
    sSockets = this;

exit:
    return error;
}

ThreadError Udp6Socket::Close()
{
    if (sSockets == this)
    {
        sSockets = sSockets->mNext;
    }
    else
    {
        for (Udp6Socket *socket = sSockets; socket; socket = socket->mNext)
        {
            if (socket->mNext == this)
            {
                socket->mNext = mNext;
                break;
            }
        }
    }

    memset(&mSockName, 0, sizeof(mSockName));
    memset(&mPeerName, 0, sizeof(mPeerName));
    mNext = NULL;

    return kThreadError_None;
}

ThreadError Udp6Socket::SendTo(Message &message, const Ip6MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Ip6MessageInfo messageInfoLocal;
    UdpHeader udpHeader;

    messageInfoLocal = messageInfo;

    if (messageInfoLocal.mSockAddr.IsUnspecified())
    {
        messageInfoLocal.mSockAddr = mSockName.mAddr;
    }

    if (mSockName.mPort == 0)
    {
        mSockName.mPort = sEphemeralPort++;
    }

    udpHeader.SetSourcePort(mSockName.mPort);
    udpHeader.SetDestinationPort(messageInfoLocal.mPeerPort);
    udpHeader.SetLength(sizeof(udpHeader) + message.GetLength());
    udpHeader.SetChecksum(0);

    SuccessOrExit(error = message.Prepend(&udpHeader, sizeof(udpHeader)));
    message.SetOffset(0);
    SuccessOrExit(error = Ip6::SendDatagram(message, messageInfoLocal, kProtoUdp));

exit:
    return error;
}

Message *Udp6::NewMessage(uint16_t reserved)
{
    return Ip6::NewMessage(sizeof(UdpHeader) + reserved);
}

ThreadError Udp6::HandleMessage(Message &message, Ip6MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    UdpHeader udpHeader;
    uint16_t payloadLength;
    uint16_t checksum;

    payloadLength = message.GetLength() - message.GetOffset();

    // check length
    VerifyOrExit(payloadLength >= sizeof(UdpHeader), error = kThreadError_Parse);

    // verify checksum
    checksum = Ip6::ComputePseudoheaderChecksum(messageInfo.mPeerAddr, messageInfo.mSockAddr,
                                                payloadLength, kProtoUdp);
    checksum = message.UpdateChecksum(checksum, message.GetOffset(), payloadLength);
    VerifyOrExit(checksum == 0xffff, ;);

    message.Read(message.GetOffset(), sizeof(udpHeader), &udpHeader);
    message.MoveOffset(sizeof(udpHeader));
    messageInfo.mPeerPort = udpHeader.GetSourcePort();
    messageInfo.mSockPort = udpHeader.GetDestinationPort();

    // find socket
    for (Udp6Socket *socket = sSockets; socket; socket = socket->mNext)
    {
        if (socket->mSockName.mPort != udpHeader.GetDestinationPort())
        {
            continue;
        }

        if (socket->mSockName.mScopeId != 0 &&
            socket->mSockName.mScopeId != messageInfo.mInterfaceId)
        {
            continue;
        }

        if (!messageInfo.mSockAddr.IsMulticast() &&
            !socket->mSockName.mAddr.IsUnspecified() &&
            socket->mSockName.mAddr != messageInfo.mSockAddr)
        {
            continue;
        }

        // verify source if connected socket
        if (socket->mPeerName.mPort != 0)
        {
            if (socket->mPeerName.mPort != udpHeader.GetSourcePort())
            {
                continue;
            }

            if (!socket->mPeerName.mAddr.IsUnspecified() &&
                socket->mPeerName.mAddr != messageInfo.mPeerAddr)
            {
                continue;
            }
        }

        socket->HandleUdpReceive(message, messageInfo);
    }

exit:
    return error;
}

ThreadError Udp6::UpdateChecksum(Message &message, uint16_t checksum)
{
    checksum = message.UpdateChecksum(checksum, message.GetOffset(), message.GetLength() - message.GetOffset());

    if (checksum != 0xffff)
    {
        checksum = ~checksum;
    }

    checksum = HostSwap16(checksum);
    message.Write(message.GetOffset() + UdpHeader::GetChecksumOffset(), sizeof(checksum), &checksum);
    return kThreadError_None;
}

}  // namespace Thread
