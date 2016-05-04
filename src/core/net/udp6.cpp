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
 *   This file implements UDP/IPv6 sockets.
 */

#include <stdio.h>

#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <net/ip6.hpp>
#include <net/udp6.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Ip6 {

UdpSocket *Udp::sSockets = NULL;
uint16_t Udp::sEphemeralPort = kDynamicPortMin;

ThreadError UdpSocket::Open(otUdpReceive aHandler, void *aContext)
{
    ThreadError error = kThreadError_None;

    for (UdpSocket *cur = Udp::sSockets; cur; cur = cur->GetNext())
    {
        if (cur == this)
        {
            ExitNow();
        }
    }

    memset(&mSockName, 0, sizeof(mSockName));
    memset(&mPeerName, 0, sizeof(mPeerName));
    mHandler = aHandler;
    mContext = aContext;

    SetNext(Udp::sSockets);
    Udp::sSockets = this;

exit:
    return error;
}

ThreadError UdpSocket::Bind(const SockAddr &aSockAddr)
{
    mSockName = aSockAddr;
    return kThreadError_None;
}

ThreadError UdpSocket::Close(void)
{
    if (Udp::sSockets == this)
    {
        Udp::sSockets = Udp::sSockets->GetNext();
    }
    else
    {
        for (UdpSocket *socket = Udp::sSockets; socket; socket = socket->GetNext())
        {
            if (socket->GetNext() == this)
            {
                socket->SetNext(GetNext());
                break;
            }
        }
    }

    memset(&mSockName, 0, sizeof(mSockName));
    memset(&mPeerName, 0, sizeof(mPeerName));
    SetNext(NULL);

    return kThreadError_None;
}

ThreadError UdpSocket::SendTo(Message &aMessage, const MessageInfo &aMessageInfo)
{
    ThreadError error = kThreadError_None;
    MessageInfo messageInfoLocal;
    UdpHeader udpHeader;

    messageInfoLocal = aMessageInfo;

    if (messageInfoLocal.GetSockAddr().IsUnspecified())
    {
        messageInfoLocal.GetSockAddr() = GetSockName().GetAddress();
    }

    if (GetSockName().mPort == 0)
    {
        GetSockName().mPort = Udp::sEphemeralPort;

        if (Udp::sEphemeralPort < Udp::kDynamicPortMax)
        {
            Udp::sEphemeralPort++;
        }
        else
        {
            Udp::sEphemeralPort = Udp::kDynamicPortMin;
        }
    }

    udpHeader.SetSourcePort(GetSockName().mPort);
    udpHeader.SetDestinationPort(messageInfoLocal.mPeerPort);
    udpHeader.SetLength(sizeof(udpHeader) + aMessage.GetLength());
    udpHeader.SetChecksum(0);

    SuccessOrExit(error = aMessage.Prepend(&udpHeader, sizeof(udpHeader)));
    aMessage.SetOffset(0);
    SuccessOrExit(error = Ip6::SendDatagram(aMessage, messageInfoLocal, kProtoUdp));

exit:
    return error;
}

Message *Udp::NewMessage(uint16_t aReserved)
{
    return Ip6::NewMessage(sizeof(UdpHeader) + aReserved);
}

ThreadError Udp::HandleMessage(Message &aMessage, MessageInfo &aMessageInfo)
{
    ThreadError error = kThreadError_None;
    UdpHeader udpHeader;
    uint16_t payloadLength;
    uint16_t checksum;

    payloadLength = aMessage.GetLength() - aMessage.GetOffset();

    // check length
    VerifyOrExit(payloadLength >= sizeof(UdpHeader), error = kThreadError_Parse);

    // verify checksum
    checksum = Ip6::ComputePseudoheaderChecksum(aMessageInfo.GetPeerAddr(), aMessageInfo.GetSockAddr(),
                                                payloadLength, kProtoUdp);
    checksum = aMessage.UpdateChecksum(checksum, aMessage.GetOffset(), payloadLength);
    VerifyOrExit(checksum == 0xffff, ;);

    aMessage.Read(aMessage.GetOffset(), sizeof(udpHeader), &udpHeader);
    aMessage.MoveOffset(sizeof(udpHeader));
    aMessageInfo.mPeerPort = udpHeader.GetSourcePort();
    aMessageInfo.mSockPort = udpHeader.GetDestinationPort();

    // find socket
    for (UdpSocket *socket = Udp::sSockets; socket; socket = socket->GetNext())
    {
        if (socket->GetSockName().mPort != udpHeader.GetDestinationPort())
        {
            continue;
        }

        if (socket->GetSockName().mScopeId != 0 &&
            socket->GetSockName().mScopeId != aMessageInfo.mInterfaceId)
        {
            continue;
        }

        if (!aMessageInfo.GetSockAddr().IsMulticast() &&
            !socket->GetSockName().GetAddress().IsUnspecified() &&
            socket->GetSockName().GetAddress() != aMessageInfo.GetSockAddr())
        {
            continue;
        }

        // verify source if connected socket
        if (socket->GetPeerName().mPort != 0)
        {
            if (socket->GetPeerName().mPort != udpHeader.GetSourcePort())
            {
                continue;
            }

            if (!socket->GetPeerName().GetAddress().IsUnspecified() &&
                socket->GetPeerName().GetAddress() != aMessageInfo.GetPeerAddr())
            {
                continue;
            }
        }

        socket->HandleUdpReceive(aMessage, aMessageInfo);
    }

exit:
    return error;
}

ThreadError Udp::UpdateChecksum(Message &aMessage, uint16_t aChecksum)
{
    aChecksum = aMessage.UpdateChecksum(aChecksum, aMessage.GetOffset(), aMessage.GetLength() - aMessage.GetOffset());

    if (aChecksum != 0xffff)
    {
        aChecksum = ~aChecksum;
    }

    aChecksum = HostSwap16(aChecksum);
    aMessage.Write(aMessage.GetOffset() + UdpHeader::GetChecksumOffset(), sizeof(aChecksum), &aChecksum);
    return kThreadError_None;
}

}  // namespace Ip6
}  // namespace Thread
