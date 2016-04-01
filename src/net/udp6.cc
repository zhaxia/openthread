/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#include <stdio.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/thread_error.h>
#include <net/ip6.h>
#include <net/udp6.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static Udp6Socket *s_sockets = NULL;
static uint16_t s_ephemeral_port = 49152;

Udp6Socket::Udp6Socket(ReceiveHandler handler, void *context)
{
    memset(&m_sockname, 0, sizeof(m_sockname));
    memset(&m_peername, 0, sizeof(m_peername));
    m_handler = handler;
    m_context = context;
    m_next = NULL;
}

ThreadError Udp6Socket::Bind(const struct sockaddr_in6 *sockaddr)
{
    ThreadError error = kThreadError_None;

    if (sockaddr)
    {
        m_sockname = *sockaddr;
    }

    for (Udp6Socket *cur = s_sockets; cur; cur = cur->m_next)
    {
        if (cur == this)
        {
            ExitNow();
        }
    }

    m_next = s_sockets;
    s_sockets = this;

exit:
    return error;
}

ThreadError Udp6Socket::Close()
{
    if (s_sockets == this)
    {
        s_sockets = s_sockets->m_next;
    }
    else
    {
        for (Udp6Socket *socket = s_sockets; socket; socket = socket->m_next)
        {
            if (socket->m_next == this)
            {
                socket->m_next = m_next;
                break;
            }
        }
    }

    memset(&m_sockname, 0, sizeof(m_sockname));
    memset(&m_peername, 0, sizeof(m_peername));
    m_next = NULL;

    return kThreadError_None;
}

ThreadError Udp6Socket::SendTo(Message &message, const Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    Ip6MessageInfo message_info_local;
    UdpHeader udp_header;

    message_info_local = message_info;

    if (message_info_local.sock_addr.IsUnspecified())
    {
        message_info_local.sock_addr = m_sockname.sin6_addr;
    }

    if (m_sockname.sin6_port == 0)
    {
        m_sockname.sin6_port = s_ephemeral_port++;
    }

    udp_header.SetSourcePort(m_sockname.sin6_port);
    udp_header.SetDestinationPort(message_info_local.peer_port);
    udp_header.SetLength(sizeof(udp_header) + message.GetLength());
    udp_header.SetChecksum(0);

    SuccessOrExit(error = message.Prepend(&udp_header, sizeof(udp_header)));
    message.SetOffset(0);
    SuccessOrExit(error = Ip6::SendDatagram(message, message_info_local, kProtoUdp));

exit:
    return error;
}

Message *Udp6::NewMessage(uint16_t reserved)
{
    return Ip6::NewMessage(sizeof(UdpHeader) + reserved);
}

ThreadError Udp6::HandleMessage(Message &message, Ip6MessageInfo &message_info)
{
    ThreadError error = kThreadError_None;
    UdpHeader udp_header;
    uint16_t payload_length;
    uint16_t checksum;

    payload_length = message.GetLength() - message.GetOffset();

    // check length
    VerifyOrExit(payload_length >= sizeof(UdpHeader), error = kThreadError_Parse);

    // verify checksum
    checksum = Ip6::ComputePseudoheaderChecksum(message_info.peer_addr, message_info.sock_addr,
                                                payload_length, kProtoUdp);
    checksum = message.UpdateChecksum(checksum, message.GetOffset(), payload_length);
    VerifyOrExit(checksum == 0xffff, ;);

    message.Read(message.GetOffset(), sizeof(udp_header), &udp_header);
    message.MoveOffset(sizeof(udp_header));
    message_info.peer_port = udp_header.GetSourcePort();
    message_info.sock_port = udp_header.GetDestinationPort();

    // find socket
    for (Udp6Socket *socket = s_sockets; socket; socket = socket->m_next)
    {
        if (socket->m_sockname.sin6_port != udp_header.GetDestinationPort())
        {
            continue;
        }

        if (socket->m_sockname.sin6_scope_id != 0 &&
            socket->m_sockname.sin6_scope_id != message_info.interface_id)
        {
            continue;
        }

        if (!message_info.sock_addr.IsMulticast() &&
            !socket->m_sockname.sin6_addr.IsUnspecified() &&
            socket->m_sockname.sin6_addr != message_info.sock_addr)
        {
            continue;
        }

        // verify source if connected socket
        if (socket->m_peername.sin6_port != 0)
        {
            if (socket->m_peername.sin6_port != udp_header.GetSourcePort())
            {
                continue;
            }

            if (!socket->m_peername.sin6_addr.IsUnspecified() &&
                socket->m_peername.sin6_addr != message_info.peer_addr)
            {
                continue;
            }
        }

        socket->HandleUdpReceive(message, message_info);
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
