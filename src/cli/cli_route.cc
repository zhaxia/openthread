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

#include <cli/cli_route.h>
#include <common/code_utils.h>
#include <stdlib.h>

namespace Thread {

static const char kName[] = "route";

CliRoute::CliRoute(CliServer *server) : CliCommand(server) {
  memset(&route_, 0, sizeof(route_));
}

const char *CliRoute::GetName() {
  return kName;
}

int CliRoute::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "usage: route\r\n"
           "  add <prefix>/<plen> <interface>\r\n");
  cur += strlen(cur);

  return cur - buf;
}

int CliRoute::AddRoute(int argc, char *argv[], char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;
  int argcur = 0;

  if (argcur >= argc)
    goto print_usage;

  char *prefix_length;
  if ((prefix_length = strchr(argv[argcur], '/')) == NULL)
    goto print_usage;
  *prefix_length++ = '\0';

  if (route_.prefix.FromString(argv[argcur]) != kThreadError_None)
    goto print_usage;

  char *endptr;
  route_.prefix_length = strtol(prefix_length, &endptr, 0);
  if (*endptr != '\0')
    goto print_usage;

  if (++argcur >= argc)
    goto print_usage;

  for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext()) {
    if (strcmp(netif->GetName(), argv[argcur]) == 0) {
      route_.interface_id = netif->GetInterfaceId();
      Ip6Routes::Add(&route_);
      ExitNow();
    }
  }

print_usage:
  cur += PrintUsage(cur, end - cur);

exit:
  return cur - buf;
}

void CliRoute::Run(int argc, char *argv[], CliServer *server) {
  char buf[512];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      goto print_usage;
    } else if (strcmp(argv[i], "add") == 0) {
      i++;
      cur += AddRoute(argc - i, &argv[i], cur, end - cur);
      goto done;
    }
  }

print_usage:
  cur += PrintUsage(cur, end - cur);

done:
  snprintf(cur, end - cur, "Done\r\n");
  cur += strlen(cur);
  server->Output(buf, cur - buf);
}

}  // namespace Thread
