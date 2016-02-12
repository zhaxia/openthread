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

#include <cli/cli_netdata.h>
#include <common/code_utils.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {

static const char kName[] = "netdata";

CliNetData::CliNetData(CliServer *server, ThreadNetif *netif) : CliCommand(server) {
  mle_ = netif->GetMle();
  network_data_ = netif->GetNetworkDataLocal();
  network_data_leader_ = netif->GetNetworkDataLeader();
}

const char *CliNetData::GetName() {
  return kName;
}

int CliNetData::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "usage: netdata\r\n"
           "  prefix add <prefix>/<plen> <domain> [pvdcsr] [prf]\r\n"
           "  prefix remove <prefix>/<plen>\r\n"
           "  route add <prefix>/<plen> [s] [prf]\r\n"
           "  route remove <prefix>/<plen>\r\n"
           "  context_reuse_delay [delay]\r\n"
           "  register\r\n");
  cur += strlen(cur);

  return cur - buf;
}

int CliNetData::PrintLocalOnMeshPrefixes(char *buf, uint16_t buf_length) {
  char *cur = buf;
#if 0
  char *end = cur + buf_length;

  const NetworkData::LocalOnMeshData *netdata = network_data_->GetLocalOnMeshPrefixes();

  while (netdata != NULL) {
    netdata->prefix.ToString(cur, end - cur);
    cur += strlen(cur);
    snprintf(cur, end - cur, "/%d ", netdata->prefix_length);
    cur += strlen(cur);
    if (netdata->slaac_preferred)
      *cur++ = 'p';
    if (netdata->slaac_valid)
      *cur++ = 'v';
    if (netdata->dhcp)
      *cur++ = 'd';
    if (netdata->dhcp_configure)
      *cur++ = 'c';
    if (netdata->stable)
      *cur++ = 's';
    if (netdata->default_route)
      *cur++ = 'r';
    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);

    netdata = netdata->next;
  }
#endif

  return cur - buf;
}

int CliNetData::PrintContextIdReuseDelay(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  snprintf(cur, end - cur, "%d\n", network_data_leader_->GetContextIdReuseDelay());
  cur += strlen(cur);

  return cur - buf;
}

int CliNetData::AddOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;
  int argcur = 0;

  Ip6Address prefix;
  uint8_t prefix_length;
  uint8_t flags = 0;
  bool stable = false;

  char *prefix_length_str;
  if ((prefix_length_str = strchr(argv[argcur], '/')) == NULL)
    goto print_usage;
  *prefix_length_str++ = '\0';

  if (prefix.FromString(argv[argcur]) != kThreadError_None)
    goto print_usage;

  char *endptr;
  prefix_length = strtol(prefix_length_str, &endptr, 0);
  if (*endptr != '\0')
    goto print_usage;

  if (++argcur < argc) {
    for (char *arg = argv[argcur]; *arg != '\0'; arg++) {
      switch (*arg) {
        case 'p': flags |= BorderRouterEntry::kPreferredFlag; break;
        case 'v': flags |= BorderRouterEntry::kValidFlag; break;
        case 'd': flags |= BorderRouterEntry::kDhcpFlag; break;
        case 'c': flags |= BorderRouterEntry::kConfigureFlag; break;
        case 'r': flags |= BorderRouterEntry::kDefaultRouteFlag; break;
        case 's': stable = true; break;
        default: goto print_usage;
      }
    }
  }

  if (network_data_->AddOnMeshPrefix(prefix.s6_addr, prefix_length, flags, stable) != kThreadError_None)
    goto print_usage;

  ExitNow();

print_usage:
  cur += PrintUsage(cur, end - cur);

exit:
  return cur - buf;
}

int CliNetData::RemoveOnMeshPrefix(int argc, char *argv[], char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;
  int argcur = 0;

  Ip6Address prefix;
  uint8_t prefix_length;

  char *prefix_length_str;
  if ((prefix_length_str = strchr(argv[argcur], '/')) == NULL)
    goto print_usage;
  *prefix_length_str++ = '\0';

  if (prefix.FromString(argv[argcur]) != kThreadError_None)
    goto print_usage;

  char *endptr;
  prefix_length = strtol(prefix_length_str, &endptr, 0);
  if (*endptr != '\0')
    goto print_usage;

  if (network_data_->RemoveOnMeshPrefix(prefix.s6_addr, prefix_length) != kThreadError_None)
    goto print_usage;

  ExitNow();

print_usage:
  cur += PrintUsage(cur, end - cur);

exit:
  return cur - buf;
}

void CliNetData::Run(int argc, char *argv[], CliServer *server) {
  char buf[512];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      goto print_usage;
    } else if (strcmp(argv[i], "prefix") == 0) {
      if (++i >= argc)
        goto print_usage;

      if (strcmp(argv[i], "add") == 0) {
        i++;
        cur += AddOnMeshPrefix(argc - i, &argv[i], cur, end - cur);
        goto done;
      } else if (strcmp(argv[i], "remove") == 0) {
        i++;
        cur += RemoveOnMeshPrefix(argc - i, &argv[i], cur, end - cur);
        goto done;
      } else if (strcmp(argv[i], "local") == 0) {
        cur += PrintLocalOnMeshPrefixes(cur, end - cur);
        goto done;
      }
      goto print_usage;
    } else if (strcmp(argv[i], "context_reuse_delay") == 0) {
      if (++i >= argc) {
        cur += PrintContextIdReuseDelay(buf, sizeof(buf));
        goto done;
      }

      uint32_t delay;
      delay = strtol(argv[i], NULL, 0);
      network_data_leader_->SetContextIdReuseDelay(delay);
      goto done;
    } else if (strcmp(argv[i], "register") == 0) {
      Ip6Address address;
      mle_->GetLeaderAddress(&address);
      network_data_->Register(&address);
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
