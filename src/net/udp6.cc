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
#include <common/thread_error.h>
#include <net/ip6.h>
#include <net/udp6.h>
#include <stdio.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static Udp6Socket *sockets_ = NULL;
static uint16_t ephemeral_port_ = 49152;

Udp6Socket::Udp6Socket(RecvFrom callback, void *context) {
  memset(&sockname_, 0, sizeof(sockname_));
  memset(&peername_, 0, sizeof(peername_));
  callback_ = callback;
  context_ = context;
  next_ = NULL;
}

ThreadError Udp6Socket::Bind(const struct sockaddr_in6 *sockaddr) {
  ThreadError error = kThreadError_None;

  if (sockaddr)
    sockname_ = *sockaddr;

  for (Udp6Socket *cur = sockets_; cur; cur = cur->next_) {
    if (cur == this)
      ExitNow();
  }

  next_ = sockets_;
  sockets_ = this;

exit:
  return error;
}

ThreadError Udp6Socket::Close() {
  if (sockets_ == this) {
    sockets_ = sockets_->next_;
  } else {
    for (Udp6Socket *socket = sockets_; socket; socket = socket->next_) {
      if (socket->next_ == this) {
        socket->next_ = next_;
        break;
      }
    }
  }

  memset(&sockname_, 0, sizeof(sockname_));
  memset(&peername_, 0, sizeof(peername_));
  next_ = NULL;

  return kThreadError_None;
}

ThreadError Udp6Socket::SendTo(Message *message, const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Ip6MessageInfo message_info_local;
  UdpHeader udp_header;

  memcpy(&message_info_local, message_info, sizeof(message_info_local));
  memset(&udp_header, 0, sizeof(udp_header));

  if (message_info_local.sock_addr.IsUnspecified())
    message_info_local.sock_addr = sockname_.sin6_addr;

  if (sockname_.sin6_port == 0)
    sockname_.sin6_port = HostSwap16(ephemeral_port_++);
  udp_header.source = sockname_.sin6_port;
  udp_header.dest = HostSwap16(message_info_local.peer_port);
  udp_header.len = HostSwap16(sizeof(udp_header) + message->GetLength());

  SuccessOrExit(error = message->Prepend(&udp_header, sizeof(udp_header)));
  message->SetOffset(0);
  SuccessOrExit(error = Ip6::SendDatagram(message, &message_info_local, IPPROTO_UDP));

exit:
  return error;
}

Message *Udp6::NewMessage(uint16_t reserved) {
  return Ip6::NewMessage(sizeof(UdpHeader) + reserved);
}

ThreadError Udp6::HandleMessage(Message *message, Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  uint16_t payload_length;
  payload_length = message->GetLength() - message->GetOffset();

  // check length
  VerifyOrExit(payload_length >= sizeof(UdpHeader), error = kThreadError_Parse);

  // verify checksum
  uint16_t checksum;
  checksum = Ip6::ComputePseudoheaderChecksum(&message_info->peer_addr, &message_info->sock_addr,
                                              payload_length, IPPROTO_UDP);
  checksum = message->UpdateChecksum(checksum, message->GetOffset(), payload_length);
  VerifyOrExit(checksum == 0xffff, ;);

  UdpHeader udp_header;
  message->Read(message->GetOffset(), sizeof(udp_header), &udp_header);
  message->MoveOffset(sizeof(udp_header));
  message_info->peer_port = HostSwap16(udp_header.source);
  message_info->sock_port = HostSwap16(udp_header.dest);

  // find socket
  for (Udp6Socket *socket = sockets_; socket; socket = socket->next_) {
    if (socket->sockname_.sin6_port != udp_header.dest)
      continue;
    if (socket->sockname_.sin6_scope_id != 0 &&
        socket->sockname_.sin6_scope_id != message_info->interface_id)
      continue;
    if (!message_info->sock_addr.IsMulticast() &&
        !socket->sockname_.sin6_addr.IsUnspecified() &&
        socket->sockname_.sin6_addr != message_info->sock_addr)
      continue;

    // verify source if connected socket
    if (socket->peername_.sin6_port != 0) {
      if (socket->peername_.sin6_port != udp_header.source)
        continue;
      if (!socket->peername_.sin6_addr.IsUnspecified() &&
          socket->peername_.sin6_addr != message_info->peer_addr)
        continue;
    }

    socket->callback_(socket->context_, message, message_info);
  }

exit:
  return error;
}

ThreadError Udp6::UpdateChecksum(Message *message, uint16_t checksum) {
  checksum = message->UpdateChecksum(checksum, message->GetOffset(), message->GetLength() - message->GetOffset());
  if (checksum != 0xffff)
    checksum = ~checksum;
  checksum = HostSwap16(checksum);
  message->Write(message->GetOffset() + offsetof(UdpHeader, check), sizeof(checksum), &checksum);
  return kThreadError_None;
}

}  // namespace Thread
