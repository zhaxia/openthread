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
#include <common/message.h>
#include <common/thread_error.h>
#include <net/icmp6.h>
#include <net/ip6.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

uint16_t Icmp6::EchoClient::next_id_ = 1;
Icmp6::EchoClient *Icmp6::echo_clients_ = NULL;
Icmp6::Callbacks *Icmp6::callbacks_ = NULL;

Icmp6::EchoClient::EchoClient(HandleEchoResponse handle_echo_response, void *context) {
  handle_echo_response_ = handle_echo_response;
  context_ = context;
  id_ = next_id_++;
  seq_ = 0;
  next_ = echo_clients_;
  echo_clients_ = this;
}

ThreadError Icmp6::EchoClient::SendEchoRequest(const struct sockaddr_in6 *sockaddr,
                                               const void *payload, uint16_t payload_length) {
  ThreadError error = kThreadError_None;
  Ip6MessageInfo message_info;
  Message *message;
  Icmp6Header icmp6_header;

  VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = message->SetLength(sizeof(icmp6_header) + payload_length));

  message->Write(sizeof(icmp6_header), payload_length, payload);

  memset(&icmp6_header, 0, sizeof(icmp6_header));
  icmp6_header.icmp6_type = ICMP6_ECHO_REQUEST;
  icmp6_header.icmp6_code = 0;
  icmp6_header.icmp6_id = HostSwap16(id_);
  icmp6_header.icmp6_seq = HostSwap16(seq_++);
  message->Write(0, sizeof(icmp6_header), &icmp6_header);

  memset(&message_info, 0, sizeof(message_info));
  message_info.peer_addr = sockaddr->sin6_addr;
  message_info.interface_id = sockaddr->sin6_scope_id;

  SuccessOrExit(error = Ip6::SendDatagram(message, &message_info, IPPROTO_ICMPV6));
  dprintf("Sent echo request\n");

 exit:
  return error;
}

ThreadError Icmp6::RegisterCallbacks(Callbacks *callbacks) {
  ThreadError error = kThreadError_None;

  for (Callbacks *cur = callbacks_; cur; cur = cur->next_) {
    if (cur == callbacks)
      ExitNow(error = kThreadError_Busy);
  }

  callbacks->next_ = callbacks_;
  callbacks_ = callbacks;

exit:
  return error;
}

ThreadError Icmp6::SendError(const Ip6Address *dst, uint8_t type, uint8_t code, const Ip6Header *ip6_header) {
  ThreadError error = kThreadError_None;
  Ip6MessageInfo message_info;
  Message *message;
  Icmp6Header icmp6_header;

  VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = message->SetLength(sizeof(icmp6_header) + sizeof(*ip6_header)));

  message->Write(sizeof(icmp6_header), sizeof(*ip6_header), ip6_header);

  memset(&icmp6_header, 0, sizeof(icmp6_header));
  icmp6_header.icmp6_type = type;
  icmp6_header.icmp6_code = code;
  message->Write(0, sizeof(icmp6_header), &icmp6_header);

  memset(&message_info, 0, sizeof(message_info));
  message_info.peer_addr = *dst;

  SuccessOrExit(error = Ip6::SendDatagram(message, &message_info, IPPROTO_ICMPV6));

  dprintf("Sent ICMPv6 Error\n");

exit:
  return error;
}

ThreadError Icmp6::HandleMessage(Message *message, Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  uint16_t payload_length;
  payload_length = message->GetLength() - message->GetOffset();

  // check length
  VerifyOrExit(payload_length >= offsetof(Icmp6Header, icmp6_dataun),  error = kThreadError_Drop);

  Icmp6Header icmp6_header;
  message->Read(message->GetOffset(), sizeof(icmp6_header), &icmp6_header);

  // verify checksum
  uint16_t checksum;
  checksum = Ip6::ComputePseudoheaderChecksum(&message_info->peer_addr, &message_info->sock_addr,
                                              payload_length, IPPROTO_ICMPV6);
  checksum = message->UpdateChecksum(checksum, message->GetOffset(), payload_length);
  VerifyOrExit(checksum == 0xffff, ;);

  switch (icmp6_header.icmp6_type) {
    case ICMP6_ECHO_REQUEST:
      return HandleEchoRequest(message, message_info);
    case ICMP6_ECHO_REPLY:
      return HandleEchoReply(message, &icmp6_header, message_info);
    case ICMP6_DST_UNREACH:
      return HandleDstUnreach(message, &icmp6_header, message_info);
  }

exit:
  return error;
}

ThreadError Icmp6::HandleDstUnreach(Message *message, const Icmp6Header *icmp6_header,
                                    const Ip6MessageInfo *message_info) {
  message->MoveOffset(sizeof(*icmp6_header));
  for (Callbacks *callbacks = callbacks_; callbacks; callbacks = callbacks->next_)
    callbacks->handle_dst_unreach_(callbacks->context_, message, icmp6_header, message_info);
  return kThreadError_None;
}

ThreadError Icmp6::HandleEchoRequest(Message *request_message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Icmp6Header icmp6_header;
  Message *reply_message;
  Ip6MessageInfo reply_message_info;

  uint16_t payload_length = request_message->GetLength() - request_message->GetOffset() -
      offsetof(Icmp6Header, icmp6_dataun);

  dprintf("Received Echo Request\n");

  memset(&icmp6_header, 0, sizeof(icmp6_header));
  icmp6_header.icmp6_type = ICMP6_ECHO_REPLY;
  icmp6_header.icmp6_code = 0;
  icmp6_header.icmp6_cksum = 0;

  VerifyOrExit((reply_message = Ip6::NewMessage(0)) != NULL, dprintf("icmp fail\n"));
  SuccessOrExit(reply_message->SetLength(offsetof(Icmp6Header, icmp6_dataun) + payload_length));

  reply_message->Write(0, offsetof(Icmp6Header, icmp6_dataun), &icmp6_header);
  request_message->CopyTo(request_message->GetOffset() + offsetof(Icmp6Header, icmp6_dataun),
                          offsetof(Icmp6Header, icmp6_dataun), payload_length,
                          reply_message);

  memset(&reply_message_info, 0, sizeof(reply_message_info));
  reply_message_info.peer_addr = message_info->peer_addr;
  if (!message_info->sock_addr.IsMulticast())
    reply_message_info.sock_addr = message_info->sock_addr;
  reply_message_info.interface_id = message_info->interface_id;

  SuccessOrExit(error = Ip6::SendDatagram(reply_message, &reply_message_info, IPPROTO_ICMPV6));

  dprintf("Sent Echo Reply\n");

exit:
  return error;
}

ThreadError Icmp6::HandleEchoReply(Message *message, const Icmp6Header *icmp6_header,
                                   const Ip6MessageInfo *message_info) {
  uint16_t id = HostSwap16(icmp6_header->icmp6_id);
  for (EchoClient *client = echo_clients_; client; client = client->next_) {
    if (client->id_ == id)
      client->handle_echo_response_(client->context_, message, message_info);
  }
  return kThreadError_None;
}

ThreadError Icmp6::UpdateChecksum(Message *message, uint16_t checksum) {
  checksum = message->UpdateChecksum(checksum, message->GetOffset(), message->GetLength() - message->GetOffset());
  if (checksum != 0xffff)
    checksum = ~checksum;
  checksum = HostSwap16(checksum);
  message->Write(message->GetOffset() + offsetof(Icmp6Header, icmp6_cksum), sizeof(checksum), &checksum);
  return kThreadError_None;
}

}  // namespace Thread
