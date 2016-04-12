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
#include <common/encoding.h>
#include <net/dhcp6_client.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {
namespace Dhcp6 {

Dhcp6Client::Dhcp6Client(Netif &netif):
    m_socket(&HandleUdpReceive, this)
{
    m_netif = &netif;
    memset(&m_identity_association, 0, sizeof(m_identity_association));
}

ThreadError Dhcp6Client::Start()
{
    ThreadError error;

    struct sockaddr_in6 sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin6_port = kUdpClientPort;
    SuccessOrExit(error = m_socket.Bind(&sockaddr));

exit:
    return error;
}

ThreadError Dhcp6Client::Stop()
{
    memset(&m_identity_association, 0, sizeof(m_identity_association));
    return kThreadError_None;
}

ThreadError Dhcp6Client::Solicit(const Ip6Address &dst, Dhcp6SolicitDelegate *delegate)
{
    Message *message;
    Ip6MessageInfo message_info;
    ThreadError error;

    m_request_type = kTypeSolicit;
    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = AppendHeader(*message, kTypeSolicit));
    SuccessOrExit(error = AppendClientIdentifier(*message));
    SuccessOrExit(error = AppendIaNa(*message, kTypeSolicit));
    SuccessOrExit(error = AppendElapsedTime(*message));
    SuccessOrExit(error = AppendOptionRequest(*message));
    SuccessOrExit(error = AppendRapidCommit(*message));

    memset(&message_info, 0, sizeof(message_info));
    memcpy(&message_info.peer_addr, &dst, sizeof(message_info.peer_addr));
    message_info.peer_port = kUdpServerPort;
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));
    m_solicit_delegate = delegate;

    dprintf("Sent DHCPv6 Solicit\n");

exit:

    if (message != NULL && error != kThreadError_None)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Dhcp6Client::Release(const Ip6Address &dst)
{
    Message *message;
    Ip6MessageInfo message_info;
    ThreadError error;

    m_request_type = kTypeRelease;
    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = AppendHeader(*message, kTypeRelease));
    SuccessOrExit(error = AppendServerIdentifier(*message));
    SuccessOrExit(error = AppendClientIdentifier(*message));
    SuccessOrExit(error = AppendElapsedTime(*message));
    SuccessOrExit(error = AppendIaNa(*message, kTypeRelease));

    memset(&message_info, 0, sizeof(message_info));
    memcpy(&message_info.peer_addr, &dst, sizeof(message_info.peer_addr));
    message_info.peer_port = kUdpServerPort;
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));

    dprintf("Sent DHCPv6 Release\n");

exit:

    if (message != NULL && error != kThreadError_None)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Dhcp6Client::LeaseQuery(const Ip6Address &eid, Dhcp6LeaseQueryDelegate *delegate)
{
    ThreadError error = kThreadError_None;
    Message *message;
    Ip6MessageInfo message_info;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = AppendHeader(*message, kTypeLeaseQuery));
    SuccessOrExit(error = AppendClientIdentifier(*message));
    SuccessOrExit(error = AppendLeaseQuery(*message, eid));
    SuccessOrExit(error = AppendElapsedTime(*message));

    memset(&message_info, 0, sizeof(message_info));
    message_info.peer_addr.s6_addr32[0] = HostSwap32(0xff030000);
    message_info.peer_addr.s6_addr32[3] = HostSwap32(0x00010003);
    message_info.peer_port = kUdpServerPort;
    message_info.interface_id = m_netif->GetInterfaceId();
    SuccessOrExit(error = m_socket.SendTo(*message, message_info));
    m_lease_query_delegate = delegate;

    dprintf("Sent DHCPv6 Lease Query\n");

exit:

    if (message != NULL && error != kThreadError_None)
    {
        Message::Free(*message);
    }

    return error;
}

bool Dhcp6Client::HaveValidLease()
{
    return m_identity_association.server.header.length != 0;
}

ThreadError Dhcp6Client::Reset()
{
    memset(&m_identity_association, 0, sizeof(m_identity_association));
    return kThreadError_None;
}

ThreadError Dhcp6Client::AppendHeader(Message &message, uint8_t type)
{
    Dhcp6Header header;

    header.type = type;
    memcpy(header.transaction_id, m_transaction_id, 3);

    return message.Append(&header, sizeof(header));
}

ThreadError Dhcp6Client::AppendServerIdentifier(Message &message)
{
    return message.Append(&m_identity_association.server, sizeof(m_identity_association.server));
}

ThreadError Dhcp6Client::AppendClientIdentifier(Message &message)
{
    ClientIdentifier option;
    LinkAddress link_address;
    ThreadError  error;

    option.header.code = HostSwap16(kOptionClientIdentifier);
    option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
    option.duid_type = HostSwap16(kDuidLinkLayerAddress);
    option.duid_hardware_type = HostSwap16(kHardwareTypeEui64);

    SuccessOrExit(error = m_netif->GetLinkAddress(link_address));
    assert(link_address.type == LinkAddress::kEui64);
    memcpy(option.duid_eui64, &link_address.address64, sizeof(option.duid_eui64));

    SuccessOrExit(error = message.Append(&option, sizeof(option)));

exit:
    return error;
}

ThreadError Dhcp6Client::AppendIaNa(Message &message, uint8_t type)
{
    ThreadError error = kThreadError_None;

    switch (type)
    {
    case kTypeSolicit:
        IaNa option;
        option.header.code = HostSwap16(kOptionIaNa);
        option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
        option.iaid = 0;
        option.t1 = 0;
        option.t2 = 0;
        SuccessOrExit(error = message.Append(&option, sizeof(option)));
        break;

    case kTypeRelease:
        SuccessOrExit(error = message.Append(&m_identity_association.ia_na, sizeof(m_identity_association.ia_na)));
        SuccessOrExit(error = message.Append(&m_identity_association.ia_address,
                                             sizeof(m_identity_association.ia_address)));
        break;

    default:
        ExitNow(error = kThreadError_Error);
    }

exit:
    return error;
}

ThreadError Dhcp6Client::AppendElapsedTime(Message &message)
{
    ElapsedTime option;

    option.header.code = HostSwap16(kOptionElapsedTime);
    option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
    option.elapsed_time = HostSwap16(0);

    return message.Append(&option, sizeof(option));
}

ThreadError Dhcp6Client::AppendOptionRequest(Message &message)
{
    OptionRequest option;

    option.header.code = HostSwap16(kOptionRequestOption);
    option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
    option.options = HostSwap16(kOptionVendorSpecificInformation);

    return message.Append(&option, sizeof(option));
}

ThreadError Dhcp6Client::AppendRapidCommit(Message &message)
{
    RapidCommit option;

    option.header.code = HostSwap16(kOptionRapidCommit);
    option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));

    return message.Append(&option, sizeof(option));
}

ThreadError Dhcp6Client::AppendLeaseQuery(Message &message, const Ip6Address &eid)
{
    LeaseQueryOption option;

    option.header.code = HostSwap16(kOptionLeaseQuery);
    option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
    option.query_type = kQueryByClientId;
    memcpy(&option.link_address, &eid, sizeof(option.link_address));

    option.ia_address.header.code = HostSwap16(kOptionIaAddress);
    option.ia_address.header.length = HostSwap16(sizeof(option.ia_address) - sizeof(option.ia_address.header));
    memcpy(&option.ia_address.address, &eid, sizeof(option.ia_address.address));
    option.ia_address.preferred_lifetime = 0xffffffff;
    option.ia_address.valid_lifetime = 0xffffffff;

    return message.Append(&option, sizeof(option));
}

void Dhcp6Client::HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info)
{
    Dhcp6Client *obj = reinterpret_cast<Dhcp6Client *>(context);
    obj->HandleUdpReceive(message, message_info);
}

void Dhcp6Client::HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info)
{
    Dhcp6Header header;

    VerifyOrExit(message.GetLength() - message.GetOffset() >= static_cast<uint16_t>(sizeof(Dhcp6Header)), ;);
    message.Read(message.GetOffset(), sizeof(header), &header);
    message.MoveOffset(sizeof(header));

    switch (header.type)
    {
    case kTypeReply:
        ProcessReply(message, message_info);
        break;

    case kTypeLeaseQueryReply:
        ProcessLeaseQueryReply(message, message_info);
        break;
    }

exit:
    {}
}

uint16_t Dhcp6Client::FindOption(Message &message, uint16_t offset, uint16_t length, uint16_t type)
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

void Dhcp6Client::ProcessReply(Message &message, const Ip6MessageInfo &message_info)
{
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();
    uint16_t option_offset;
    ServerIdentifier server_identifier;

    dprintf("Received DHCPv6 Reply\n");

    // Server Identifier
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionServerIdentifier)) > 0, ;);
    SuccessOrExit(ProcessServerIdentifier(message, option_offset, &server_identifier));

    // Client Identifier
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0, ;);
    SuccessOrExit(ProcessClientIdentifier(message, option_offset));

    switch (m_request_type)
    {
    case kTypeSolicit:
        // Rapid Commit
        VerifyOrExit(FindOption(message, offset, length, kOptionRapidCommit) > 0, ;);

        // IA_NA
        VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaNa)) > 0, ;);
        SuccessOrExit(ProcessIaNa(message, option_offset));

        // Vendor-specific
        VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionVendorSpecificInformation)) > 0, ;);
        SuccessOrExit(ProcessVendorSpecificInformation(message, option_offset));

        memcpy(&m_identity_association.server, &server_identifier, sizeof(m_identity_association.server));
        break;

    case kTypeRelease:
        memset(&m_identity_association, 0, sizeof(m_identity_association));
        break;
    }

exit:
    {}
}

void Dhcp6Client::ProcessLeaseQueryReply(Message &message, const Ip6MessageInfo &message_info)
{
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();
    uint16_t option_offset;

    dprintf("Received DHCPv6 Lease Query Reply\n");

    // Server Identifier
    VerifyOrExit(FindOption(message, offset, length, kOptionServerIdentifier) > 0, ;);

    // Client Identifier
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0, ;);
    SuccessOrExit(ProcessClientIdentifier(message, option_offset));

    // Client Data
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientData)) > 0, ;);
    SuccessOrExit(ProcessClientData(message, option_offset));

exit:
    {}
}

ThreadError Dhcp6Client::ProcessClientIdentifier(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    LinkAddress link_address;
    ClientIdentifier option;

    m_netif->GetLinkAddress(link_address);
    assert(link_address.type == LinkAddress::kEui64);

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option) &&
                 option.header.length == HostSwap16(sizeof(option) - sizeof(option.header)) &&
                 option.duid_type == HostSwap16(kDuidLinkLayerAddress) &&
                 option.duid_hardware_type == HostSwap16(kHardwareTypeEui64) &&
                 memcmp(option.duid_eui64, &link_address.address64, sizeof(option.duid_eui64)) == 0,
                 error = kThreadError_Parse);

exit:
    return error;
}

ThreadError Dhcp6Client::ProcessServerIdentifier(Message &message, uint16_t offset, ServerIdentifier *option)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(message.Read(offset, sizeof(*option), option)  == sizeof(*option) &&
                 option->header.length == HostSwap16(sizeof(*option) - sizeof(Dhcp6Option)) &&
                 option->duid_type == HostSwap16(kDuidLinkLayerAddress) &&
                 option->duid_hardware_type == HostSwap16(kHardwareTypeEui64),
                 error = kThreadError_Parse);

exit:
    return error;
}

ThreadError Dhcp6Client::ProcessIaNa(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    IaNa option;
    uint16_t option_offset;
    uint16_t length;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option), error = kThreadError_Parse);
    offset += sizeof(option);
    length = HostSwap16(option.header.length);

    if ((option_offset = FindOption(message, offset, length, kOptionStatusCode)) > 0)
    {
        SuccessOrExit(error = ProcessStatusCode(message, option_offset));
    }

    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaAddress)) > 0, ;);
    SuccessOrExit(error = ProcessIaAddr(message, option_offset));
    memcpy(&m_identity_association.ia_na, &option, sizeof(m_identity_association.ia_na));

exit:
    return error;
}

ThreadError Dhcp6Client::ProcessIaAddr(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    IaAddress option;
    uint32_t preferred_lifetime;
    uint32_t valid_lifetime;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option) &&
                 option.header.length == HostSwap16(sizeof(option) - sizeof(option.header)),
                 error = kThreadError_Parse);

    preferred_lifetime = HostSwap32(option.preferred_lifetime);
    valid_lifetime = HostSwap32(option.valid_lifetime);
    VerifyOrExit(preferred_lifetime <= valid_lifetime, error = kThreadError_Parse);

    SuccessOrExit(error = m_solicit_delegate->HandleIaAddr(&option));
    memcpy(&m_identity_association.ia_address, &option, sizeof(m_identity_association.ia_address));

exit:
    return error;
}

ThreadError Dhcp6Client::ProcessStatusCode(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    StatusCode option;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option) &&
                 option.header.length == HostSwap16(sizeof(option) - sizeof(option.header)) &&
                 option.status_code == HostSwap16(kStatusSuccess),
                 error = kThreadError_Parse);

exit:
    return error;
}

ThreadError Dhcp6Client::ProcessVendorSpecificInformation(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    VendorSpecificInformation option;
    uint8_t buf[128];
    uint16_t buf_length;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option), error = kThreadError_Parse);
    buf_length = HostSwap16(option.header.length);

    VerifyOrExit(buf_length >= sizeof(option) - sizeof(option.header) && buf_length <= sizeof(buf) &&
                 message.Read(offset + sizeof(option), buf_length, buf) == buf_length,
                 error = kThreadError_Parse);

    SuccessOrExit(error = m_solicit_delegate->HandleVendorSpecificInformation(HostSwap32(option.enterprise_number),
                                                                              buf, buf_length));

exit:
    return error;
}

ThreadError Dhcp6Client::ProcessClientData(Message &message, uint16_t offset)
{
    ThreadError error = kThreadError_None;
    ClientData option;
    uint16_t length;
    uint16_t option_offset;

    VerifyOrExit(message.Read(offset, sizeof(option), &option) == sizeof(option), error = kThreadError_Parse);
    offset += sizeof(option);
    length = HostSwap16(option.header.length);

    ClientIdentifier client_id;
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0 &&
                 message.Read(option_offset, sizeof(client_id), &client_id) == sizeof(client_id) &&
                 client_id.header.length == HostSwap16(sizeof(client_id) - sizeof(client_id.header)) &&
                 client_id.duid_type == HostSwap16(kDuidLinkLayerAddress) &&
                 client_id.duid_hardware_type == HostSwap16(kHardwareTypeEui64),
                 error = kThreadError_Parse);

    IaAddress eid;
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaAddress)) > 0 &&
                 message.Read(option_offset, sizeof(eid), &eid) == sizeof(eid) &&
                 eid.header.length == HostSwap16(sizeof(eid) - sizeof(eid.header)),
                 error = kThreadError_Parse);

    IaAddress rloc;
    option_offset += sizeof(rloc);
    VerifyOrExit((option_offset = FindOption(message, option_offset, length - (option_offset - offset),
                                             kOptionIaAddress)) > 0 &&
                 message.Read(option_offset, sizeof(rloc), &rloc) == sizeof(rloc) &&
                 rloc.header.length == HostSwap16(sizeof(rloc) - sizeof(rloc.header)),
                 error = kThreadError_Parse);

    ClientLastTransactionTime time;
    VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientLastTransactionTime)) > 0 &&
                 message.Read(option_offset, sizeof(time), &time) == sizeof(time) &&
                 time.header.length == HostSwap16(sizeof(time) - sizeof(time.header)),
                 error = kThreadError_Parse);

    Mac::Address64 address64;
    memcpy(&address64, client_id.duid_eui64, sizeof(address64));
    m_lease_query_delegate->HandleLeaseQueryReply(&eid.address, &rloc.address, time.last_transaction_time);

exit:
    return error;
}

}  // namespace Dhcp6
}  // namespace Thread
