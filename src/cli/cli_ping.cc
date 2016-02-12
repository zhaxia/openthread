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

#include <cli/cli_ping.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <net/ip6_address.h>
#include <stdlib.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static const char kName[] = "ping";
static sockaddr_in6 dstaddr_;
static CliServer *server_;

CliPing::CliPing(CliServer *server):
    CliCommand(server),
    echo_client_(&HandleEchoResponse, this) {
}

const char *CliPing::GetName() {
  return kName;
}

int CliPing::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur, "usage: ping [-I interface] [-i wait] [-c count] [-s size] host\r\n");
  cur += strlen(cur);

  return cur - buf;
}

void CliPing::EchoRequest() {
  uint8_t buf[2048];

  for (int i = 0; i < length_; i++)
    buf[i] = i;

  echo_client_.SendEchoRequest(&dstaddr_, buf, length_);
}

void CliPing::Run(int argc, char *argv[], CliServer *server) {
  char buf[128];
  char *cur = buf;
  char *end = cur + sizeof(buf);
  char *endptr;

  server_ = server;
  length_ = 0;
  memset(&dstaddr_, 0, sizeof(dstaddr_));

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      ExitNow();
    } else if (strcmp(argv[i], "-I") == 0) {
      Netif *netif;
      VerifyOrExit(++i < argc, ;);
      VerifyOrExit((netif = Netif::GetNetifByName(argv[i])) != NULL, ;);
      dstaddr_.sin6_scope_id = netif->GetInterfaceId();
    } else if (strcmp(argv[i], "-s") == 0) {
      VerifyOrExit(++i < argc, ;);
      length_ = strtol(argv[i], &endptr, 0);
      VerifyOrExit(*endptr == '\0', ;);
    } else {
      VerifyOrExit(dstaddr_.sin6_addr.FromString(argv[i]) == kThreadError_None, ;);
      EchoRequest();
      return;
    }
  }

exit:
  cur += PrintUsage(cur, end - cur);
  snprintf(cur, end - cur, "Done\r\n");
  cur += strlen(cur);
  server->Output(buf, cur - buf);
}

void CliPing::HandleEchoResponse(void *context, Message *message, const Ip6MessageInfo *message_info) {
  CliPing *obj = reinterpret_cast<CliPing*>(context);
  obj->HandleEchoResponse(message, message_info);
}

void CliPing::HandleEchoResponse(Message *message, const Ip6MessageInfo *message_info) {
  char buf[256];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  Icmp6Header icmp6_header;
  message->Read(message->GetOffset(), sizeof(icmp6_header), &icmp6_header);

  snprintf(cur, end - cur, "%d bytes from ", message->GetLength() - message->GetOffset());
  cur += strlen(cur);

  message_info->peer_addr.ToString(cur, end - cur);
  cur += strlen(cur);

  Netif *netif = Netif::GetNetifById(message_info->interface_id);
  snprintf(cur, end - cur, "%%%s: icmp_seq=%d hlim=%d",
           netif->GetName(), HostSwap16(icmp6_header.icmp6_seq), message_info->hop_limit);
  cur += strlen(cur);

  snprintf(cur, end - cur, "\r\n");
  cur += strlen(cur);

  server_->Output(buf, cur - buf);
}

}  // namespace Thread
