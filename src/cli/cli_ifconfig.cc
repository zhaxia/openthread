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

#include <cli/cli_ifconfig.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <net/ip6_address.h>
#include <net/netif.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static const char kName[] = "ifconfig";

CliIfconfig::CliIfconfig(CliServer *server) : CliCommand(server) {
}

const char *CliIfconfig::GetName() {
  return kName;
}

int PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur, "usage: ifconfig\r\n");
  cur += strlen(cur);

  return cur - buf;
}

int PrintStatus(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext()) {
    snprintf(cur, end - cur, "%s:\r\n", netif->GetName());
    cur += strlen(cur);

    for (const NetifAddress *addr = netif->GetAddresses(); addr; addr = addr->next_) {
      snprintf(cur, end - cur,  "  inet6 ");
      cur += strlen(cur);

      addr->address.ToString(cur, end - cur);
      cur += strlen(cur);

      snprintf(cur, end - cur, "/%d\r\n", addr->prefix_length);
      cur += strlen(cur);
    }
  }

  return cur - buf;
}

void CliIfconfig::Run(int argc, char *argv[], CliServer *server) {
  char buf[512];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  if (argc > 0)
    goto print_usage;

  cur += PrintStatus(buf, sizeof(buf));
  goto done;

print_usage:
  cur += PrintUsage(cur, end - cur);

done:
  snprintf(cur, end - cur, "Done\r\n");
  cur += strlen(cur);
  server->Output(buf, cur - buf);
}

}  // namespace Thread
