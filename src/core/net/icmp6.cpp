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
namespace Ip6 {

uint16_t IcmpEcho::sNextId = 1;
IcmpEcho *IcmpEcho::sEchoClients = NULL;
IcmpHandler *IcmpHandler::sHandlers = NULL;

IcmpEcho::IcmpEcho(EchoReplyHandler aHandler, void *aContext)
{
    mHandler = aHandler;
    mContext = aContext;
    mId = sNextId++;
    mSeq = 0;
    mNext = sEchoClients;
    sEchoClients = this;
}

ThreadError IcmpEcho::SendEchoRequest(const SockAddr &aDestination,
                                      const void *aPayload, uint16_t aPayloadLength)
{
    ThreadError error = kThreadError_None;
    MessageInfo messageInfo;
    Message *message;
    IcmpHeader icmp6Header;

    VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(sizeof(icmp6Header) + aPayloadLength));

    message->Write(sizeof(icmp6Header), aPayloadLength, aPayload);

    icmp6Header.Init();
    icmp6Header.SetType(IcmpHeader::kTypeEchoRequest);
    icmp6Header.SetId(mId);
    icmp6Header.SetSequence(mSeq++);
    message->Write(0, sizeof(icmp6Header), &icmp6Header);

    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.GetPeerAddr() = aDestination.GetAddress();
    messageInfo.mInterfaceId = aDestination.mScopeId;

    SuccessOrExit(error = Ip6::SendDatagram(*message, messageInfo, kProtoIcmp6));
    dprintf("Sent echo request\n");

exit:
    return error;
}

ThreadError Icmp::RegisterCallbacks(IcmpHandler &aHandler)
{
    ThreadError error = kThreadError_None;

    for (IcmpHandler *cur = IcmpHandler::sHandlers; cur; cur = cur->mNext)
    {
        if (cur == &aHandler)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    aHandler.mNext = IcmpHandler::sHandlers;
    IcmpHandler::sHandlers = &aHandler;

exit:
    return error;
}

ThreadError Icmp::SendError(const Address &aDestination, IcmpHeader::Type aType, IcmpHeader::Code aCode,
                            const Header &aHeader)
{
    ThreadError error = kThreadError_None;
    MessageInfo messageInfo;
    Message *message;
    IcmpHeader icmp6Header;

    VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(sizeof(icmp6Header) + sizeof(aHeader)));

    message->Write(sizeof(icmp6Header), sizeof(aHeader), &aHeader);

    icmp6Header.Init();
    icmp6Header.SetType(aType);
    icmp6Header.SetCode(aCode);
    message->Write(0, sizeof(icmp6Header), &icmp6Header);

    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.mPeerAddr = aDestination;

    SuccessOrExit(error = Ip6::SendDatagram(*message, messageInfo, kProtoIcmp6));

    dprintf("Sent ICMPv6 Error\n");

exit:
    return error;
}

ThreadError Icmp::HandleMessage(Message &aMessage, MessageInfo &aMessageInfo)
{
    ThreadError error = kThreadError_None;
    uint16_t payloadLength;
    IcmpHeader icmp6Header;
    uint16_t checksum;

    payloadLength = aMessage.GetLength() - aMessage.GetOffset();

    // check length
    VerifyOrExit(payloadLength >= IcmpHeader::GetDataOffset(),  error = kThreadError_Drop);
    aMessage.Read(aMessage.GetOffset(), sizeof(icmp6Header), &icmp6Header);

    // verify checksum
    checksum = Ip6::ComputePseudoheaderChecksum(aMessageInfo.GetPeerAddr(), aMessageInfo.GetSockAddr(),
                                                payloadLength, kProtoIcmp6);
    checksum = aMessage.UpdateChecksum(checksum, aMessage.GetOffset(), payloadLength);
    VerifyOrExit(checksum == 0xffff, ;);

    switch (icmp6Header.GetType())
    {
    case IcmpHeader::kTypeEchoRequest:
        return HandleEchoRequest(aMessage, aMessageInfo);

    case IcmpHeader::kTypeEchoReply:
        return HandleEchoReply(aMessage, aMessageInfo, icmp6Header);

    case IcmpHeader::kTypeDstUnreach:
        return HandleDstUnreach(aMessage, aMessageInfo, icmp6Header);
    }

exit:
    return error;
}

ThreadError Icmp::HandleDstUnreach(Message &aMessage, const MessageInfo &aMessageInfo,
                                   const IcmpHeader &aIcmpheader)
{
    aMessage.MoveOffset(sizeof(aIcmpheader));

    for (IcmpHandler *handler = IcmpHandler::sHandlers; handler; handler = handler->mNext)
    {
        handler->HandleDstUnreach(aMessage, aMessageInfo, aIcmpheader);
    }

    return kThreadError_None;
}

ThreadError Icmp::HandleEchoRequest(Message &aRequestMessage, const MessageInfo &aMessageInfo)
{
    ThreadError error = kThreadError_None;
    IcmpHeader icmp6Header;
    Message *replyMessage;
    MessageInfo replyMessageInfo;
    uint16_t payloadLength;

    payloadLength = aRequestMessage.GetLength() - aRequestMessage.GetOffset() - IcmpHeader::GetDataOffset();

    dprintf("Received Echo Request\n");

    icmp6Header.Init();
    icmp6Header.SetType(IcmpHeader::kTypeEchoReply);

    VerifyOrExit((replyMessage = Ip6::NewMessage(0)) != NULL, dprintf("icmp fail\n"));
    SuccessOrExit(replyMessage->SetLength(IcmpHeader::GetDataOffset() + payloadLength));

    replyMessage->Write(0, IcmpHeader::GetDataOffset(), &icmp6Header);
    aRequestMessage.CopyTo(aRequestMessage.GetOffset() + IcmpHeader::GetDataOffset(),
                           IcmpHeader::GetDataOffset(), payloadLength, *replyMessage);

    memset(&replyMessageInfo, 0, sizeof(replyMessageInfo));
    replyMessageInfo.GetPeerAddr() = aMessageInfo.GetPeerAddr();

    if (!aMessageInfo.GetSockAddr().IsMulticast())
    {
        replyMessageInfo.GetSockAddr() = aMessageInfo.GetSockAddr();
    }

    replyMessageInfo.mInterfaceId = aMessageInfo.mInterfaceId;

    SuccessOrExit(error = Ip6::SendDatagram(*replyMessage, replyMessageInfo, kProtoIcmp6));

    dprintf("Sent Echo Reply\n");

exit:
    return error;
}

ThreadError Icmp::HandleEchoReply(Message &aMessage, const MessageInfo &aMessageInfo,
                                  const IcmpHeader &aIcmpheader)
{
    uint16_t id = aIcmpheader.GetId();

    for (IcmpEcho *client = IcmpEcho::sEchoClients; client; client = client->mNext)
    {
        if (client->mId == id)
        {
            client->HandleEchoReply(aMessage, aMessageInfo);
        }
    }

    return kThreadError_None;
}

ThreadError Icmp::UpdateChecksum(Message &aMessage, uint16_t aChecksum)
{
    aChecksum = aMessage.UpdateChecksum(aChecksum, aMessage.GetOffset(),
                                        aMessage.GetLength() - aMessage.GetOffset());

    if (aChecksum != 0xffff)
    {
        aChecksum = ~aChecksum;
    }

    aChecksum = HostSwap16(aChecksum);
    aMessage.Write(aMessage.GetOffset() + IcmpHeader::GetChecksumOffset(), sizeof(aChecksum), &aChecksum);
    return kThreadError_None;
}

}  // namespace Ip6
}  // namespace Thread
