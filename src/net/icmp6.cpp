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

#include <common/code_utils.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/icmp6.h>
#include <net/ip6.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

uint16_t Icmp6Echo::s_next_id = 1;
Icmp6Echo *Icmp6Echo::s_echo_clients = NULL;
Icmp6Handler *Icmp6Handler::s_handlers = NULL;

Icmp6Echo::Icmp6Echo(EchoReplyHandler handler, void *context)
{
    m_handler = handler;
    m_context = context;
    m_id = s_next_id++;
    m_seq = 0;
    m_next = s_echo_clients;
    s_echo_clients = this;
}

ThreadError Icmp6Echo::SendEchoRequest(const struct sockaddr_in6 &sockaddr,
                                       const void *payload, uint16_t payload_length)
{
    ThreadError error = kThreadError_None;
    Ip6MessageInfo message_info;
    Message *message;
    Icmp6Header icmp6_header;

    VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(sizeof(icmp6_header) + payload_length));

    message->Write(sizeof(icmp6_header), payload_length, payload);

    icmp6_header.Init();
    icmp6_header.SetType(Icmp6Header::kTypeEchoRequest);
    icmp6_header.SetId(m_id);
    icmp6_header.SetSequence(m_seq++);
    message->Write(0, sizeof(icmp6_header), &icmp6_header);

    memset(&message_info, 0, sizeof(message_info));
    message_info.peer_addr = sockaddr.sin6_addr;
    message_info.interface_id = sockaddr.sin6_scope_id;

    SuccessOrExit(error = Ip6::SendDatagram(*message, message_info, kProtoIcmp6));
    dprintf("Sent echo request\n");

exit:
    return error;
}

ThreadError Icmp6::RegisterCallbacks(Icmp6Handler &handler)
{
    ThreadError error = kThreadError_None;

    for (Icmp6Handler *cur = Icmp6Handler::s_handlers; cur; cur = cur->m_next)
    {
        if (cur == &handler)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    handler.m_next = Icmp6Handler::s_handlers;
    Icmp6Handler::s_handlers = &handler;

exit:
    return error;
}

ThreadError Icmp6::SendError(const Ip6Address &dst, Icmp6Header::Type type, Icmp6Header::Code code,
                             const Ip6Header &ip6_header)
{
    ThreadError error = kThreadError_None;
    Ip6MessageInfo message_info;
    Message *message;
    Icmp6Header icmp6_header;

    VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->SetLength(sizeof(icmp6_header) + sizeof(ip6_header)));

    message->Write(sizeof(icmp6_header), sizeof(ip6_header), &ip6_header);

    icmp6_header.Init();
    icmp6_header.SetType(type);
    icmp6_header.SetCode(code);
    message->Write(0, sizeof(icmp6_header), &icmp6_header);

    memset(&message_info, 0, sizeof(message_info));
    message_info.peer_addr = dst;

    SuccessOrExit(error = Ip6::SendDatagram(*message, message_info, kProtoIcmp6));

    dprintf("Sent ICMPv6 Error\n");

exit:
    return error;
}

ThreadError Icmp6::HandleMessage(Message &message, Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    uint16_t payload_length;
    Icmp6Header icmp6_header;
    uint16_t checksum;

    payload_length = message.GetLength() - message.GetOffset();

    // check length
    VerifyOrExit(payload_length >= Icmp6Header::GetDataOffset(),  error = kThreadError_Drop);
    message.Read(message.GetOffset(), sizeof(icmp6_header), &icmp6_header);

    // verify checksum
    checksum = Ip6::ComputePseudoheaderChecksum(message_info.peer_addr, message_info.sock_addr,
                                                payload_length, kProtoIcmp6);
    checksum = message.UpdateChecksum(checksum, message.GetOffset(), payload_length);
    VerifyOrExit(checksum == 0xffff, ;);

    switch (icmp6_header.GetType())
    {
    case Icmp6Header::kTypeEchoRequest:
        return HandleEchoRequest(message, message_info);

    case Icmp6Header::kTypeEchoReply:
        return HandleEchoReply(message, message_info, icmp6_header);

    case Icmp6Header::kTypeDstUnreach:
        return HandleDstUnreach(message, message_info, icmp6_header);
    }

exit:
    return error;
}

ThreadError Icmp6::HandleDstUnreach(Message &message, const Ip6MessageInfo &message_info,
                                    const Icmp6Header &icmp6_header)
{
    message.MoveOffset(sizeof(icmp6_header));

    for (Icmp6Handler *handler = Icmp6Handler::s_handlers; handler; handler = handler->m_next)
    {
        handler->HandleDstUnreach(message, message_info, icmp6_header);
    }

    return kThreadError_None;
}

ThreadError Icmp6::HandleEchoRequest(Message &request_message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Icmp6Header icmp6_header;
    Message *reply_message;
    Ip6MessageInfo reply_message_info;
    uint16_t payload_length;

    payload_length = request_message.GetLength() - request_message.GetOffset() - Icmp6Header::GetDataOffset();

    dprintf("Received Echo Request\n");

    icmp6_header.Init();
    icmp6_header.SetType(Icmp6Header::kTypeEchoReply);

    VerifyOrExit((reply_message = Ip6::NewMessage(0)) != NULL, dprintf("icmp fail\n"));
    SuccessOrExit(reply_message->SetLength(Icmp6Header::GetDataOffset() + payload_length));

    reply_message->Write(0, Icmp6Header::GetDataOffset(), &icmp6_header);
    request_message.CopyTo(request_message.GetOffset() + Icmp6Header::GetDataOffset(),
                           Icmp6Header::GetDataOffset(), payload_length, *reply_message);

    memset(&reply_message_info, 0, sizeof(reply_message_info));
    reply_message_info.peer_addr = message_info.peer_addr;

    if (!message_info.sock_addr.IsMulticast())
    {
        reply_message_info.sock_addr = message_info.sock_addr;
    }

    reply_message_info.interface_id = message_info.interface_id;

    SuccessOrExit(error = Ip6::SendDatagram(*reply_message, reply_message_info, kProtoIcmp6));

    dprintf("Sent Echo Reply\n");

exit:
    return error;
}

ThreadError Icmp6::HandleEchoReply(Message &message, const Ip6MessageInfo &message_info,
                                   const Icmp6Header &icmp6_header)
{
    uint16_t id = icmp6_header.GetId();

    for (Icmp6Echo *client = Icmp6Echo::s_echo_clients; client; client = client->m_next)
    {
        if (client->m_id == id)
        {
            client->HandleEchoReply(message, message_info);
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
