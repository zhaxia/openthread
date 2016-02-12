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

#include <cli/cli_ip.h>
#include <common/code_utils.h>
#include <stdlib.h>

namespace Thread {

static const char kName[] = "ip";

CliIp::CliIp(CliServer *server) : CliCommand(server) {
}

const char *CliIp::GetName() {
  return kName;
}

int CliIp::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "usage: ip\r\n"
           "  addr add <addr> dev <dev>\r\n"
           "  addr del <addr> dev <dev>\r\n");
  cur += strlen(cur);

  return cur - buf;
}

ThreadError CliIp::AddAddress(int argc, char *argv[]) {
  ThreadError error = kThreadError_Error;
  int argcur = 0;

  SuccessOrExit(error = address_.address.FromString(argv[argcur]));
  VerifyOrExit(++argcur < argc, error = kThreadError_Parse);

  VerifyOrExit(strcmp(argv[argcur], "dev") == 0, error = kThreadError_Parse);
  VerifyOrExit(++argcur < argc, error = kThreadError_Parse);

  for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext()) {
    if (strcmp(netif->GetName(), argv[argcur]) == 0) {
      address_.prefix_length = 64;
      address_.preferred_lifetime = 0xffffffff;
      address_.valid_lifetime = 0xffffffff;
      netif->AddAddress(&address_);
      ExitNow(error = kThreadError_None);
    }
  }

exit:
  return error;
}

ThreadError CliIp::DeleteAddress(int argc, char *argv[]) {
  ThreadError error;
  int argcur = 0;

  Ip6Address address;

  SuccessOrExit(error = address.FromString(argv[argcur]));
  VerifyOrExit(++argcur < argc, error = kThreadError_Parse);
  VerifyOrExit(address == address_.address, error = kThreadError_Error);

  VerifyOrExit(strcmp(argv[argcur], "dev") == 0, error = kThreadError_Parse);
  VerifyOrExit(++argcur < argc, error = kThreadError_Parse);

  for (Netif *netif = Netif::GetNetifList(); netif; netif = netif->GetNext()) {
    if (strcmp(netif->GetName(), argv[argcur]) == 0) {
      SuccessOrExit(error = netif->RemoveAddress(&address_));
      ExitNow(error = kThreadError_None);
    }
  }

exit:
  return error;
}

void CliIp::Run(int argc, char *argv[], CliServer *server) {
  char buf[512];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      goto print_usage;
    } else if (strcmp(argv[i], "addr") == 0) {
      if (++i >= argc)
        goto print_usage;

      if (strcmp(argv[i], "add") == 0) {
        i++;
        if (AddAddress(argc - i, &argv[i]) != kThreadError_None)
          goto print_usage;
        goto done;
      } else if (strcmp(argv[i], "del") == 0) {
        i++;
        if (DeleteAddress(argc - i, &argv[i]) != kThreadError_None)
          goto print_usage;
        goto done;
      }
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
