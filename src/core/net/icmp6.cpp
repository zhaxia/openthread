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
 *   This file implements ICMPv6.
 */

#include <common/code_utils.hpp>
#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/icmp6.hpp>
#include <net/ip6.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

uint16_t Icmp6Echo::sNextId = 1;
Icmp6Echo *Icmp6Echo::sEchoClients = NULL;
Icmp6Handler *Icmp6Handler::sHandlers = NULL;

Icmp6Echo::Icmp6Echo(EchoReplyHandler handler, void *context)
{
    mHandler = handler;
    mContext = context;
    mId = sNextId++;
    mSeq = 0;
    mNext = sEchoClients;
    sEchoClients = this;
}

ThreadError Icmp6Echo::SendEchoRequest(const SockAddr &sockaddr,
                                       const void *payload, uint16_t payloadLength)
{
    ThreadError error = kThreadError_None;
    Ip6MessageInfo messageInfo;
    Message *message;
    Icmp6Header icmp6Header;

    VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(sizeof(icmp6Header) + payloadLength));

    message->Write(sizeof(icmp6Header), payloadLength, payload);

    icmp6Header.Init();
    icmp6Header.SetType(Icmp6Header::kTypeEchoRequest);
    icmp6Header.SetId(mId);
    icmp6Header.SetSequence(mSeq++);
    message->Write(0, sizeof(icmp6Header), &icmp6Header);

    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.GetPeerAddr() = sockaddr.GetAddress();
    messageInfo.mInterfaceId = sockaddr.mScopeId;

    SuccessOrExit(error = Ip6::SendDatagram(*message, messageInfo, kProtoIcmp6));
    dprintf("Sent echo request\n");

exit:
    return error;
}

ThreadError Icmp6::RegisterCallbacks(Icmp6Handler &handler)
{
    ThreadError error = kThreadError_None;

    for (Icmp6Handler *cur = Icmp6Handler::sHandlers; cur; cur = cur->mNext)
    {
        if (cur == &handler)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    handler.mNext = Icmp6Handler::sHandlers;
    Icmp6Handler::sHandlers = &handler;

exit:
    return error;
}

ThreadError Icmp6::SendError(const Ip6Address &dst, Icmp6Header::Type type, Icmp6Header::Code code,
                             const Ip6Header &ip6Header)
{
    ThreadError error = kThreadError_None;
    Ip6MessageInfo messageInfo;
    Message *message;
    Icmp6Header icmp6Header;

    VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(sizeof(icmp6Header) + sizeof(ip6Header)));

    message->Write(sizeof(icmp6Header), sizeof(ip6Header), &ip6Header);

    icmp6Header.Init();
    icmp6Header.SetType(type);
    icmp6Header.SetCode(code);
    message->Write(0, sizeof(icmp6Header), &icmp6Header);

    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.mPeerAddr = dst;

    SuccessOrExit(error = Ip6::SendDatagram(*message, messageInfo, kProtoIcmp6));

    dprintf("Sent ICMPv6 Error\n");

exit:
    return error;
}

ThreadError Icmp6::HandleMessage(Message &message, Ip6MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    uint16_t payloadLength;
    Icmp6Header icmp6Header;
    uint16_t checksum;

    payloadLength = message.GetLength() - message.GetOffset();

    // check length
    VerifyOrExit(payloadLength >= Icmp6Header::GetDataOffset(),  error = kThreadError_Drop);
    message.Read(message.GetOffset(), sizeof(icmp6Header), &icmp6Header);

    // verify checksum
    checksum = Ip6::ComputePseudoheaderChecksum(messageInfo.GetPeerAddr(), messageInfo.GetSockAddr(),
                                                payloadLength, kProtoIcmp6);
    checksum = message.UpdateChecksum(checksum, message.GetOffset(), payloadLength);
    VerifyOrExit(checksum == 0xffff, ;);

    switch (icmp6Header.GetType())
    {
    case Icmp6Header::kTypeEchoRequest:
        return HandleEchoRequest(message, messageInfo);

    case Icmp6Header::kTypeEchoReply:
        return HandleEchoReply(message, messageInfo, icmp6Header);

    case Icmp6Header::kTypeDstUnreach:
        return HandleDstUnreach(message, messageInfo, icmp6Header);
    }

exit:
    return error;
}

ThreadError Icmp6::HandleDstUnreach(Message &message, const Ip6MessageInfo &messageInfo,
                                    const Icmp6Header &icmp6Header)
{
    message.MoveOffset(sizeof(icmp6Header));

    for (Icmp6Handler *handler = Icmp6Handler::sHandlers; handler; handler = handler->mNext)
    {
        handler->HandleDstUnreach(message, messageInfo, icmp6Header);
    }

    return kThreadError_None;
}

ThreadError Icmp6::HandleEchoRequest(Message &requestMessage, const Ip6MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Icmp6Header icmp6Header;
    Message *replyMessage;
    Ip6MessageInfo replyMessageInfo;
    uint16_t payloadLength;

    payloadLength = requestMessage.GetLength() - requestMessage.GetOffset() - Icmp6Header::GetDataOffset();

    dprintf("Received Echo Request\n");

    icmp6Header.Init();
    icmp6Header.SetType(Icmp6Header::kTypeEchoReply);

    VerifyOrExit((replyMessage = Ip6::NewMessage(0)) != NULL, dprintf("icmp fail\n"));
    SuccessOrExit(replyMessage->SetLength(Icmp6Header::GetDataOffset() + payloadLength));

    replyMessage->Write(0, Icmp6Header::GetDataOffset(), &icmp6Header);
    requestMessage.CopyTo(requestMessage.GetOffset() + Icmp6Header::GetDataOffset(),
                          Icmp6Header::GetDataOffset(), payloadLength, *replyMessage);

    memset(&replyMessageInfo, 0, sizeof(replyMessageInfo));
    replyMessageInfo.GetPeerAddr() = messageInfo.GetPeerAddr();

    if (!messageInfo.GetSockAddr().IsMulticast())
    {
        replyMessageInfo.GetSockAddr() = messageInfo.GetSockAddr();
    }

    replyMessageInfo.mInterfaceId = messageInfo.mInterfaceId;

    SuccessOrExit(error = Ip6::SendDatagram(*replyMessage, replyMessageInfo, kProtoIcmp6));

    dprintf("Sent Echo Reply\n");

exit:
    return error;
}

ThreadError Icmp6::HandleEchoReply(Message &message, const Ip6MessageInfo &messageInfo,
                                   const Icmp6Header &icmp6Header)
{
    uint16_t id = icmp6Header.GetId();

    for (Icmp6Echo *client = Icmp6Echo::sEchoClients; client; client = client->mNext)
    {
        if (client->mId == id)
        {
            client->HandleEchoReply(message, messageInfo);
        }
    }

    return kThreadError_None;
}

ThreadError Icmp6::UpdateChecksum(Message &message, uint16_t checksum)
{
    checksum = message.UpdateChecksum(checksum, message.GetOffset(), message.GetLength() - message.GetOffset());

    if (checksum != 0xffff)
    {
        checksum = ~checksum;
    }

    checksum = HostSwap16(checksum);
    message.Write(message.GetOffset() + Icmp6Header::GetChecksumOffset(), sizeof(checksum), &checksum);
    return kThreadError_None;
}

}  // namespace Thread
