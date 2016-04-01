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

#include <common/code_utils.h>
#include <common/encoding.h>
#include <net/dhcp6_server.h>
#include <thread/mle.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {
namespace Dhcp6 {

Dhcp6Server::Dhcp6Server(Netif &netif):
    m_socket(&HandleUdpReceive, this)
{
    m_netif = &netif;
}

ThreadError Dhcp6Server::Start(const Ip6Address *address, Dhcp6ServerDelegate *delegate)
{
    ThreadError error;

    struct sockaddr_in6 sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    memcpy(&sockaddr.sin6_addr, address, sizeof(sockaddr.sin6_addr));
    sockaddr.sin6_port = kUdpServerPort;
    SuccessOrExit(error = m_socket.Bind(&sockaddr));

    // m_netif->SubscribeAllDhcpServersMulticast();

    m_delegate = delegate;

exit:
    return error;
}

void Dhcp6Server::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    Dhcp6Server *obj = reinterpret_cast<Dhcp6Server *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void Dhcp6Server::HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info)
{
    Dhcp6Header header;

    VerifyOrExit(message.GetLength() - message.GetOffset() >= static_cast<uint16_t>(sizeof(Dhcp6Header)), ;);
    message.Read(message.GetOffset(), sizeof(header), &header);
    message.MoveOffset(sizeof(header));

    switch (header.type)
    {
    case kTypeSolicit:
        ProcessSolicit(message, &message_info.peer_addr, header.transaction_id);
        break;

    case kTypeRelease:
        ProcessRelease(message, &message_info.peer_addr, header.transaction_id);
        break;

    case kTypeLeaseQuery:
        ProcessLeaseQuery(message, &message_info.peer_addr, header.transaction_id);
        break;
    }

exit:
    {}
}

uint16_t Dhcp6Server::FindOption(Message &message, uint16_t offset, uint16_t length, uint16_t type)
{
    uint16_t end = offset + length;

    while (offset <= end)
    {
        Dhcp6Option option;
        message.Read(offset, sizeof(option), &option);

        if (option.code == HostSwap16(type))
        {
            return offset;
        }

        offset += sizeof(option) + HostSwap16(option.length);
    }

    return 0;
}

void Dhcp6Server::ProcessSolicit(Message &message, const Ip6Address *address, const uint8_t *transaction_id)
{
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();
    uint16_t option_offset;

    ClientIdentifier client_identifier;

    dprintf("Received DHCPv6 Solicit\n");

    // Client Identifier (discard if not present)
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0, ;);
    SuccessOrExit(ProcessClientIdentifier(message, option_offset, &client_identifier));

    // Server Identifier (assuming Rapid Commit, discard if present)
    VerifyOrExit(FindOption(message, offset, length, kOptionServerIdentifier) == 0, ;);

    // Rapid Commit (assuming Rapid Commit, discard if not present)
    VerifyOrExit(FindOption(message, offset, length, kOptionRapidCommit) > 0, ;);

    // IA_NA (discard if not present)
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaNa)) > 0, ;);

    // Vendor-specific (discard if not present)
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionRequestOption)) > 0, ;);
    SuccessOrExit(ProcessRequestOption(message, option_offset));

    SuccessOrExit(SendReply(address, kTypeSolicit, transaction_id, &client_identifier));

exit:
    {}
}

void Dhcp6Server::ProcessRelease(Message &message, const Ip6Address *address, const uint8_t *transaction_id)
{
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();
    uint16_t option_offset;

    ClientIdentifier client_identifier;

    dprintf("Received DHCPv6 Release\n");

    // Client Identifier (discard if not present)
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0, ;);
    SuccessOrExit(ProcessClientIdentifier(message, option_offset, &client_identifier));

    // Server Identifier (discard if not present)
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionServerIdentifier)) > 0, ;);
    SuccessOrExit(ProcessServerIdentifier(message, option_offset));

    // IA_NA (discard if not present)
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaNa)) > 0, ;);
    SuccessOrExit(ProcessIaNa(message, option_offset));

    SuccessOrExit(SendReply(address, kTypeRelease, transaction_id, &client_identifier));

exit:
    {}
}

void Dhcp6Server::ProcessLeaseQuery(Message &message, const Ip6Address *source, const uint8_t *transaction_id)
{
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();
    uint16_t option_offset;

    ClientIdentifier client_identifier;

    dprintf("Received DHCPv6 Lease Query\n");

    // Client Identifier (discard if not present)
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0, ;);
    SuccessOrExit(ProcessClientIdentifier(message, option_offset, &client_identifier));

    // Server Identifier (optional)
    if ((option_offset = FindOption(message, offset, length, kOptionServerIdentifier)) > 0)
    {
        SuccessOrExit(ProcessServerIdentifier(message, option_offset));
    }

    // Lease Query
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionLeaseQuery)) > 0, ;);
    LeaseQueryOption option;
    VerifyOrExit(message.Read(option_offset, sizeof(option), &option) == sizeof(option) &&
                 option.header.length == HostSwap16(sizeof(option) - sizeof(option.header)) &&
                 option.query_type == kQueryByClientId &&
                 option.ia_address.header.code == HostSwap16(kOptionIaAddress) &&
                 option.ia_address.header.length == HostSwap16(sizeof(option.ia_address) -
                                                               sizeof(option.ia_address.header)), ;);

    Ip6Address address;
    uint32_t last_transaction_time;

    if (m_delegate->HandleLeaseQuery(&option.ia_address.address, &address, &last_transaction_time) == kThreadError_None)
    {
        SendLeaseQueryReply(source, transaction_id, &client_identifier,
                            &option.ia_address.address, &address, last_transaction_time);
    }

exit:
    {}
}

ThreadError Dhcp6Server::ProcessClientIdentifier(Message &message, uint16_t offset, ClientIdentifier *option)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(message.Read(offset, sizeof(*option), option) == sizeof(*option) &&
                 option->header.length == HostSwap16(sizeof(*option) - sizeof(Dhcp6Option)) &&
                 option->duid_type == HostSwap16(kDuidLinkLayerAddress) &&
                 option->duid_hardware_type == HostSwap16(kHardwareTypeEui64),
                 error = kThreadError_Parse);

exit:
    return error;
}

ThreadError Dhcp6Server::ProcessServerIdentifier(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    LinkAddress link_address;
    ServerIdentifier option;

    m_netif->GetLinkAddress(link_address);
    assert(link_address.type == LinkAddress::kEui64);

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option) &&
                 option.header.length == HostSwap16(sizeof(option) - sizeof(Dhcp6Option)) &&
                 option.duid_type == HostSwap16(kDuidLinkLayerAddress) &&
                 option.duid_hardware_type == HostSwap16(kHardwareTypeEui64) &&
                 memcmp(option.duid_eui64, &link_address.address64, sizeof(option.duid_eui64)) == 0,
                 error = kThreadError_Parse);

exit:
    return error;
}

ThreadError Dhcp6Server::ProcessIaNa(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    IaNa option;
    uint16_t option_offset;
    uint16_t length;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option), error = kThreadError_Parse);
    offset += sizeof(option);
    length = HostSwap16(option.header.length);
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaAddress)) > 0, ;);
    SuccessOrExit(error = ProcessIaAddr(message, option_offset));

exit:
    return error;
}

ThreadError Dhcp6Server::ProcessIaAddr(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    IaAddress option;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option) &&
                 option.header.length == HostSwap16(sizeof(option) - sizeof(Dhcp6Option)),
                 error = kThreadError_Parse);

    SuccessOrExit(error = m_delegate->HandleReleaseAddress(&option.address));

exit:
    return error;
}

ThreadError Dhcp6Server::ProcessRequestOption(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    OptionRequest option;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option) &&
                 option.header.length == HostSwap16(sizeof(option) - sizeof(Dhcp6Option)) &&
                 option.options == HostSwap16(kOptionVendorSpecificInformation),
                 error = kThreadError_Parse);

exit:
    return error;
}

ThreadError Dhcp6Server::SendReply(const Ip6Address *address, uint8_t type, const uint8_t *transaction_id,
                                   const ClientIdentifier *client_identifier)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = AppendHeader(*message, kTypeReply, transaction_id));
    SuccessOrExit(error = AppendServerIdentifier(*message));
    SuccessOrExit(error = AppendClientIdentifier(*message, client_identifier));

    switch (type)
    {
    case kTypeSolicit:
        SuccessOrExit(error = AppendIaNa(*message, client_identifier));
        SuccessOrExit(error = AppendRapidCommit(*message));
        break;

    case kTypeRelease:
        break;

    default:
        ExitNow(error = kThreadError_Error);
    }

    Ip6MessageInfo message_info;
    memset(&message_info, 0, sizeof(message_info));
    memcpy(&message_info.peer_addr, address, sizeof(message_info.peer_addr));
    message_info.peer_port = kUdpClientPort;
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent DHCPv6 Reply\n");

exit:

    if (message != NULL && error != kThreadError_None)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Dhcp6Server::SendLeaseQueryReply(const Ip6Address *dst, const uint8_t *transaction_id,
                                             const ClientIdentifier *client_identifier,
                                             const Ip6Address *eid, const Ip6Address *rloc,
                                             uint32_t transaction_time)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = AppendHeader(*message, kTypeLeaseQueryReply, transaction_id));
    SuccessOrExit(error = AppendServerIdentifier(*message));
    SuccessOrExit(error = AppendClientIdentifier(*message, client_identifier));

    ClientData client_data;
    IaAddress ia_address;
    ClientLastTransactionTime last_transaction_time;

    client_data.header.code = HostSwap16(kOptionClientData);
    client_data.header.length = HostSwap16(sizeof(*client_identifier) +
                                           sizeof(ia_address) + sizeof(ia_address) +
                                           sizeof(last_transaction_time));
    SuccessOrExit(error = message->Append(&client_data, sizeof(client_data)));
    SuccessOrExit(error = message->Append(client_identifier, sizeof(*client_identifier)));

    ia_address.header.code = HostSwap16(kOptionIaAddress);
    ia_address.header.length = HostSwap16(sizeof(ia_address) - sizeof(ia_address.header));
    memcpy(&ia_address.address, eid, sizeof(ia_address.address));
    ia_address.preferred_lifetime = 0xffffffff;
    ia_address.valid_lifetime = 0xffffffff;
    SuccessOrExit(error = message->Append(&ia_address, sizeof(ia_address)));

    ia_address.header.code = HostSwap16(kOptionIaAddress);
    ia_address.header.length = HostSwap16(sizeof(ia_address) - sizeof(ia_address.header));
    memcpy(&ia_address.address, rloc, sizeof(ia_address.address));
    ia_address.preferred_lifetime = 0xffffffff;
    ia_address.valid_lifetime = 0xffffffff;
    SuccessOrExit(error = message->Append(&ia_address, sizeof(ia_address)));

    last_transaction_time.header.code = HostSwap16(kOptionClientLastTransactionTime);
    last_transaction_time.header.length = HostSwap16(sizeof(last_transaction_time) -
                                                     sizeof(last_transaction_time.header));
    last_transaction_time.last_transaction_time = HostSwap32(transaction_time);
    SuccessOrExit(error = message->Append(&last_transaction_time, sizeof(last_transaction_time)));

    Ip6MessageInfo message_info;
    memset(&message_info, 0, sizeof(message_info));
    memcpy(&message_info.peer_addr, dst, sizeof(message_info.peer_addr));
    message_info.peer_port = kUdpClientPort;
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent DHCPv6 Lease Query Reply\n");

exit:

    if (message != NULL && error != kThreadError_None)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Dhcp6Server::AppendHeader(Message &message, uint8_t type, const uint8_t *transaction_id)
{
    Dhcp6Header header;

    header.type = type;
    memcpy(header.transaction_id, transaction_id, sizeof(header.transaction_id));

    return message.Append(&header, sizeof(header));
}

ThreadError Dhcp6Server::AppendClientIdentifier(Message &message, const ClientIdentifier *client_identifier)
{
    ThreadError error;

    SuccessOrExit(error = message.Append(client_identifier, sizeof(*client_identifier)));

exit:
    return error;
}

ThreadError Dhcp6Server::AppendServerIdentifier(Message &message)
{
    ServerIdentifier option;
    LinkAddress link_address;
    ThreadError  error;

    option.header.code = HostSwap16(kOptionServerIdentifier);
    option.header.length = HostSwap16(sizeof(option) - sizeof(Dhcp6Option));
    option.duid_type = HostSwap16(kDuidLinkLayerAddress);
    option.duid_hardware_type = HostSwap16(kHardwareTypeEui64);

    SuccessOrExit(error = m_netif->GetLinkAddress(link_address));
    assert(link_address.type == LinkAddress::kEui64);
    memcpy(option.duid_eui64, &link_address.address64, sizeof(option.duid_eui64));

    SuccessOrExit(error = message.Append(&option, sizeof(option)));

exit:
    return error;
}

ThreadError Dhcp6Server::AppendIaNa(Message &message, const ClientIdentifier *client_identifier)
{
    ThreadError error;
    IaNa ia_na;
    IaAddress ia_address;

    ia_na.iaid = 0;
    ia_na.t1 = 0;
    ia_na.t2 = 0;

    if (m_delegate->HandleGetAddress(client_identifier, &ia_address) == kThreadError_None)
    {
        ia_na.header.code = HostSwap16(kOptionIaNa);
        ia_na.header.length = HostSwap16(sizeof(ia_na) - sizeof(Dhcp6Option) + sizeof(ia_address));
        SuccessOrExit(error = message.Append(&ia_na, sizeof(ia_na)));

        ia_address.header.code = HostSwap16(kOptionIaAddress);
        ia_address.header.length = HostSwap16(sizeof(ia_address) - sizeof(Dhcp6Option));
        SuccessOrExit(error = message.Append(&ia_address, sizeof(ia_address)));
    }
    else
    {
        ia_na.header.code = HostSwap16(kOptionIaNa);
        ia_na.header.length = HostSwap16(sizeof(ia_na) - sizeof(Dhcp6Option) + sizeof(StatusCode));
        SuccessOrExit(error = message.Append(&ia_na, sizeof(ia_na)));
        SuccessOrExit(error = AppendStatusCode(message, kStatusNoAddrsAvail));
    }

exit:
    return error;
}

ThreadError Dhcp6Server::AppendStatusCode(Message &message, uint16_t status_code)
{
    StatusCode option;

    option.header.code = HostSwap16(kOptionStatusCode);
    option.header.length = HostSwap16(sizeof(option) - sizeof(Dhcp6Option));
    option.status_code = HostSwap16(status_code);

    return message.Append(&option, sizeof(option));
}

ThreadError Dhcp6Server::AppendRapidCommit(Message &message)
{
    RapidCommit option;

    option.header.code = HostSwap16(kOptionRapidCommit);
    option.header.length = HostSwap16(sizeof(option) - sizeof(Dhcp6Option));

    return message.Append(&option, sizeof(option));
}

}  // namespace Dhcp6
}  // namespace Thread
