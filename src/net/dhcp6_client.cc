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
#include <net/dhcp6_client.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {
namespace Dhcp6 {

Dhcp6Client::Dhcp6Client(Netif *netif):
    socket_(&RecvFrom, this) {
  netif_ = netif;
  memset(&identity_association_, 0, sizeof(identity_association_));
}

ThreadError Dhcp6Client::Start() {
  ThreadError error;

  struct sockaddr_in6 sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin6_port = HostSwap16(kUdpClientPort);
  SuccessOrExit(error = socket_.Bind(&sockaddr));

exit:
  return error;
}

ThreadError Dhcp6Client::Stop() {
  memset(&identity_association_, 0, sizeof(identity_association_));
  return kThreadError_None;
}

ThreadError Dhcp6Client::Solicit(const Ip6Address *dst, Dhcp6SolicitDelegate *delegate) {
  Message *message;
  Ip6MessageInfo message_info;
  ThreadError error;

  request_type_ = kTypeSolicit;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = AppendHeader(message, kTypeSolicit));
  SuccessOrExit(error = AppendClientIdentifier(message));
  SuccessOrExit(error = AppendIaNa(message, kTypeSolicit));
  SuccessOrExit(error = AppendElapsedTime(message));
  SuccessOrExit(error = AppendOptionRequest(message));
  SuccessOrExit(error = AppendRapidCommit(message));

  memset(&message_info, 0, sizeof(message_info));
  memcpy(&message_info.peer_addr, dst, sizeof(message_info.peer_addr));
  message_info.peer_port = kUdpServerPort;
  SuccessOrExit(error = socket_.SendTo(message, &message_info));
  solicit_delegate_ = delegate;

  dprintf("Sent DHCPv6 Solicit\n");

exit:
  if (message != NULL && error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError Dhcp6Client::Release(const Ip6Address *dst) {
  Message *message;
  Ip6MessageInfo message_info;
  ThreadError error;

  request_type_ = kTypeRelease;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = AppendHeader(message, kTypeRelease));
  SuccessOrExit(error = AppendServerIdentifier(message));
  SuccessOrExit(error = AppendClientIdentifier(message));
  SuccessOrExit(error = AppendElapsedTime(message));
  SuccessOrExit(error = AppendIaNa(message, kTypeRelease));

  memset(&message_info, 0, sizeof(message_info));
  memcpy(&message_info.peer_addr, dst, sizeof(message_info.peer_addr));
  message_info.peer_port = kUdpServerPort;
  SuccessOrExit(error = socket_.SendTo(message, &message_info));

  dprintf("Sent DHCPv6 Release\n");

exit:
  if (message != NULL && error != kThreadError_None)
    Message::Free(message);
  return error;
}

ThreadError Dhcp6Client::LeaseQuery(const Ip6Address *eid, Dhcp6LeaseQueryDelegate *delegate) {
  ThreadError error = kThreadError_None;
  Message *message;
  Ip6MessageInfo message_info;

  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = AppendHeader(message, kTypeLeaseQuery));
  SuccessOrExit(error = AppendClientIdentifier(message));
  SuccessOrExit(error = AppendLeaseQuery(message, eid));
  SuccessOrExit(error = AppendElapsedTime(message));

  memset(&message_info, 0, sizeof(message_info));
  message_info.peer_addr.s6_addr32[0] = HostSwap32(0xff030000);
  message_info.peer_addr.s6_addr32[3] = HostSwap32(0x00010003);
  message_info.peer_port = kUdpServerPort;
  message_info.interface_id = netif_->GetInterfaceId();
  SuccessOrExit(error = socket_.SendTo(message, &message_info));
  lease_query_delegate_ = delegate;

  dprintf("Sent DHCPv6 Lease Query\n");

exit:
  if (message != NULL && error != kThreadError_None)
    Message::Free(message);
  return error;
}

bool Dhcp6Client::HaveValidLease() {
  return identity_association_.server.header.length != 0;
}

ThreadError Dhcp6Client::Reset() {
  memset(&identity_association_, 0, sizeof(identity_association_));
  return kThreadError_None;
}

ThreadError Dhcp6Client::AppendHeader(Message *message, uint8_t type) {
  Dhcp6Header header;

  header.type = type;
  memcpy(header.transaction_id, transaction_id_, 3);

  return message->Append(&header, sizeof(header));
}

ThreadError Dhcp6Client::AppendServerIdentifier(Message *message) {
  return message->Append(&identity_association_.server, sizeof(identity_association_.server));
}

ThreadError Dhcp6Client::AppendClientIdentifier(Message *message) {
  ClientIdentifier option;
  LinkAddress link_address;
  ThreadError  error;

  option.header.code = HostSwap16(kOptionClientIdentifier);
  option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
  option.duid_type = HostSwap16(kDuidLinkLayerAddress);
  option.duid_hardware_type = HostSwap16(kHardwareTypeEui64);

  SuccessOrExit(error = netif_->GetLinkAddress(&link_address));
  assert(link_address.type == LinkAddress::kEui64);
  memcpy(option.duid_eui64, &link_address.address64, sizeof(option.duid_eui64));

  SuccessOrExit(error = message->Append(&option, sizeof(option)));

exit:
  return error;
}

ThreadError Dhcp6Client::AppendIaNa(Message *message, uint8_t type) {
  ThreadError error = kThreadError_None;

  switch (type) {
    case kTypeSolicit:
      IaNa option;
      option.header.code = HostSwap16(kOptionIaNa);
      option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
      option.iaid = 0;
      option.t1 = 0;
      option.t2 = 0;
      SuccessOrExit(error = message->Append(&option, sizeof(option)));
      break;
    case kTypeRelease:
      SuccessOrExit(error = message->Append(&identity_association_.ia_na, sizeof(identity_association_.ia_na)));
      SuccessOrExit(error = message->Append(&identity_association_.ia_address,
                                            sizeof(identity_association_.ia_address)));
      break;
    default:
      ExitNow(error = kThreadError_Error);
  }

exit:
  return error;
}

ThreadError Dhcp6Client::AppendElapsedTime(Message *message) {
  ElapsedTime option;

  option.header.code = HostSwap16(kOptionElapsedTime);
  option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
  option.elapsed_time = HostSwap16(0);

  return message->Append(&option, sizeof(option));
}

ThreadError Dhcp6Client::AppendOptionRequest(Message *message) {
  OptionRequest option;

  option.header.code = HostSwap16(kOptionRequestOption);
  option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
  option.options = HostSwap16(kOptionVendorSpecificInformation);

  return message->Append(&option, sizeof(option));
}

ThreadError Dhcp6Client::AppendRapidCommit(Message *message) {
  RapidCommit option;

  option.header.code = HostSwap16(kOptionRapidCommit);
  option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));

  return message->Append(&option, sizeof(option));
}

ThreadError Dhcp6Client::AppendLeaseQuery(Message *message, const Ip6Address *eid) {
  LeaseQueryOption option;

  option.header.code = HostSwap16(kOptionLeaseQuery);
  option.header.length = HostSwap16(sizeof(option) - sizeof(option.header));
  option.query_type = kQueryByClientId;
  memcpy(&option.link_address, eid, sizeof(option.link_address));

  option.ia_address.header.code = HostSwap16(kOptionIaAddress);
  option.ia_address.header.length = HostSwap16(sizeof(option.ia_address) - sizeof(option.ia_address.header));
  memcpy(&option.ia_address.address, eid, sizeof(option.ia_address.address));
  option.ia_address.preferred_lifetime = 0xffffffff;
  option.ia_address.valid_lifetime = 0xffffffff;

  return message->Append(&option, sizeof(option));
}

void Dhcp6Client::RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info) {
  Dhcp6Client *obj = reinterpret_cast<Dhcp6Client*>(context);
  obj->RecvFrom(message, message_info);
}

void Dhcp6Client::RecvFrom(Message *message, const Ip6MessageInfo *message_info) {
  Dhcp6Header header;

  VerifyOrExit(message->GetLength() - message->GetOffset() >= static_cast<uint16_t>(sizeof(Dhcp6Header)), ;);
  message->Read(message->GetOffset(), sizeof(header), &header);
  message->MoveOffset(sizeof(header));

  switch (header.type) {
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

uint16_t Dhcp6Client::FindOption(Message *message, uint16_t offset, uint16_t length, uint16_t type) {
  uint16_t end = offset + length;

  while (offset <= end) {
    Dhcp6Option option;
    message->Read(offset, sizeof(option), &option);
    if (option.code == HostSwap16(type))
      return offset;
    offset += sizeof(option) + HostSwap16(option.length);
  }

  return 0;
}

void Dhcp6Client::ProcessReply(Message *message, const Ip6MessageInfo *message_info) {
  uint16_t offset = message->GetOffset();
  uint16_t length = message->GetLength() - message->GetOffset();
  uint16_t option_offset;
  ServerIdentifier server_identifier;

  dprintf("Received DHCPv6 Reply\n");

  // Server Identifier
  VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionServerIdentifier)) > 0, ;);
  SuccessOrExit(ProcessServerIdentifier(message, option_offset, &server_identifier));

  // Client Identifier
  VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0, ;);
  SuccessOrExit(ProcessClientIdentifier(message, option_offset));

  switch (request_type_) {
    case kTypeSolicit:
      // Rapid Commit
      VerifyOrExit(FindOption(message, offset, length, kOptionRapidCommit) > 0, ;);

      // IA_NA
      VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaNa)) > 0, ;);
      SuccessOrExit(ProcessIaNa(message, option_offset));

      // Vendor-specific
      VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionVendorSpecificInformation)) > 0, ;);
      SuccessOrExit(ProcessVendorSpecificInformation(message, option_offset));

      memcpy(&identity_association_.server, &server_identifier, sizeof(identity_association_.server));
      break;
    case kTypeRelease:
      memset(&identity_association_, 0, sizeof(identity_association_));
      break;
  }

exit:
  {}
}

void Dhcp6Client::ProcessLeaseQueryReply(Message *message, const Ip6MessageInfo *message_info) {
  uint16_t offset = message->GetOffset();
  uint16_t length = message->GetLength() - message->GetOffset();
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

ThreadError Dhcp6Client::ProcessClientIdentifier(Message *message, uint16_t offset) {
  ThreadError error = kThreadError_None;
  LinkAddress link_address;
  ClientIdentifier option;

  netif_->GetLinkAddress(&link_address);
  assert(link_address.type == LinkAddress::kEui64);

  VerifyOrExit(message->Read(offset, sizeof(option), &option) == sizeof(option) &&
               option.header.length == HostSwap16(sizeof(option) - sizeof(option.header)) &&
               option.duid_type == HostSwap16(kDuidLinkLayerAddress) &&
               option.duid_hardware_type == HostSwap16(kHardwareTypeEui64) &&
               memcmp(option.duid_eui64, &link_address.address64, sizeof(option.duid_eui64)) == 0,
               error = kThreadError_Parse);

exit:
  return error;
}

ThreadError Dhcp6Client::ProcessServerIdentifier(Message *message, uint16_t offset, ServerIdentifier *option) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(message->Read(offset, sizeof(*option), option)  == sizeof(*option) &&
               option->header.length == HostSwap16(sizeof(*option) - sizeof(Dhcp6Option)) &&
               option->duid_type == HostSwap16(kDuidLinkLayerAddress) &&
               option->duid_hardware_type == HostSwap16(kHardwareTypeEui64),
               error = kThreadError_Parse);

exit:
  return error;
}

ThreadError Dhcp6Client::ProcessIaNa(Message *message, uint16_t offset) {
  ThreadError error = kThreadError_None;
  IaNa option;
  uint16_t option_offset;
  uint16_t length;

  VerifyOrExit(message->Read(offset, sizeof(option), &option) == sizeof(option), error = kThreadError_Parse);
  offset += sizeof(option);
  length = HostSwap16(option.header.length);
  if ((option_offset = FindOption(message, offset, length, kOptionStatusCode)) > 0)
    SuccessOrExit(error = ProcessStatusCode(message, option_offset));

  VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaAddress)) > 0, ;);
  SuccessOrExit(error = ProcessIaAddr(message, option_offset));
  memcpy(&identity_association_.ia_na, &option, sizeof(identity_association_.ia_na));

exit:
  return error;
}

ThreadError Dhcp6Client::ProcessIaAddr(Message *message, uint16_t offset) {
  ThreadError error = kThreadError_None;
  IaAddress option;
  uint32_t preferred_lifetime;
  uint32_t valid_lifetime;

  VerifyOrExit(message->Read(offset, sizeof(option), &option) == sizeof(option) &&
               option.header.length == HostSwap16(sizeof(option) - sizeof(option.header)),
               error = kThreadError_Parse);

  preferred_lifetime = HostSwap32(option.preferred_lifetime);
  valid_lifetime = HostSwap32(option.valid_lifetime);
  VerifyOrExit(preferred_lifetime <= valid_lifetime, error = kThreadError_Parse);

  SuccessOrExit(error = solicit_delegate_->HandleIaAddr(&option));
  memcpy(&identity_association_.ia_address, &option, sizeof(identity_association_.ia_address));

exit:
  return error;
}

ThreadError Dhcp6Client::ProcessStatusCode(Message *message, uint16_t offset) {
  ThreadError error = kThreadError_None;
  StatusCode option;

  VerifyOrExit(message->Read(offset, sizeof(option), &option) == sizeof(option) &&
               option.header.length == HostSwap16(sizeof(option) - sizeof(option.header)) &&
               option.status_code == HostSwap16(kStatusSuccess),
               error = kThreadError_Parse);

exit:
  return error;
}

ThreadError Dhcp6Client::ProcessVendorSpecificInformation(Message *message, uint16_t offset) {
  ThreadError error = kThreadError_None;
  VendorSpecificInformation option;
  uint8_t buf[128];
  uint16_t buf_length;

  VerifyOrExit(message->Read(offset, sizeof(option), &option) == sizeof(option), error = kThreadError_Parse);
  buf_length = HostSwap16(option.header.length);

  VerifyOrExit(buf_length >= sizeof(option) - sizeof(option.header) && buf_length <= sizeof(buf) &&
               message->Read(offset + sizeof(option), buf_length, buf) == buf_length,
               error = kThreadError_Parse);

  SuccessOrExit(error = solicit_delegate_->HandleVendorSpecificInformation(HostSwap32(option.enterprise_number),
                                                                           buf, buf_length));

exit:
  return error;
}

ThreadError Dhcp6Client::ProcessClientData(Message *message, uint16_t offset) {
  ThreadError error = kThreadError_None;
  ClientData option;
  uint16_t length;
  uint16_t option_offset;

  VerifyOrExit(message->Read(offset, sizeof(option), &option) == sizeof(option), error = kThreadError_Parse);
  offset += sizeof(option);
  length = HostSwap16(option.header.length);

  ClientIdentifier client_id;
  VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientIdentifier)) > 0 &&
               message->Read(option_offset, sizeof(client_id), &client_id) == sizeof(client_id) &&
               client_id.header.length == HostSwap16(sizeof(client_id) - sizeof(client_id.header)) &&
               client_id.duid_type == HostSwap16(kDuidLinkLayerAddress) &&
               client_id.duid_hardware_type == HostSwap16(kHardwareTypeEui64),
               error = kThreadError_Parse);

  IaAddress eid;
  VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionIaAddress)) > 0 &&
               message->Read(option_offset, sizeof(eid), &eid) == sizeof(eid) &&
               eid.header.length == HostSwap16(sizeof(eid) - sizeof(eid.header)),
               error = kThreadError_Parse);

  IaAddress rloc;
  option_offset += sizeof(rloc);
  VerifyOrExit((option_offset = FindOption(message, option_offset, length - (option_offset - offset),
                                           kOptionIaAddress)) > 0 &&
               message->Read(option_offset, sizeof(rloc), &rloc) == sizeof(rloc) &&
               rloc.header.length == HostSwap16(sizeof(rloc) - sizeof(rloc.header)),
               error = kThreadError_Parse);

  ClientLastTransactionTime time;
  VerifyOrExit((option_offset = FindOption(message, offset, length, kOptionClientLastTransactionTime)) > 0 &&
               message->Read(option_offset, sizeof(time), &time) == sizeof(time) &&
               time.header.length == HostSwap16(sizeof(time) - sizeof(time.header)),
               error = kThreadError_Parse);

  MacAddr64 address64;
  memcpy(&address64, client_id.duid_eui64, sizeof(address64));
  lease_query_delegate_->HandleLeaseQueryReply(&eid.address, &rloc.address, time.last_transaction_time);

exit:
  return error;
}

}  // namespace Dhcp6
}  // namespace Thread
