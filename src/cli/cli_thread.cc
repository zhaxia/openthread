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

#include <cli/cli.h>
#include <cli/cli_thread.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static const char kName[] = "thread";

CliThread::CliThread(CliServer *server, ThreadNetif *netif) : CliCommand(server) {
  netif_ = netif;
  mle_ = netif->GetMle();
}

const char *CliThread::GetName() {
  return kName;
}

int CliThread::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "usage: thread\r\n"
           "  cache\r\n"
           "  children\r\n"
           "  key <key>\r\n"
           "  key_sequence [sequence]\r\n"
           "  leader_data\r\n"
           "  mode [rsdn]\r\n"
           "  network_id_timeout [timeout]\r\n"
           "  release_router [router_id]\r\n"
           "  router_uprade_threshold [threshold]\r\n"
           "  routers\r\n"
           "  routes\r\n"
           "  start\r\n"
           "  state [detached|child|router|leader]\r\n"
           "  stop\r\n"
           "  timeout [timeout]\r\n"
           "  weight [weight]\r\n");
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintAddressCache(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  int count = 0;
  const AddressResolver *resolver = netif_->GetAddressResolver();
  uint16_t num_entries;
  const AddressResolver::Cache *entries = resolver->GetCacheEntries(&num_entries);

  for (int i = 0; i < num_entries; i++) {
    if (entries[i].state == AddressResolver::Cache::kStateInvalid)
      continue;
    entries[i].target.ToString(cur, end - cur);
    cur += strlen(cur);
    snprintf(cur, end - cur, " %d ", entries[i].state);
    cur += strlen(cur);
    snprintf(cur, end - cur, "%04x ", entries[i].rloc);
    cur += strlen(cur);
    snprintf(cur, end - cur, "%d\r\n", entries[i].timeout);
    cur += strlen(cur);
    count++;
  }

  snprintf(cur, end - cur, "Total: %d\r\n", count);
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintChildren(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  int count = 0;
  Child *children;
  uint8_t num_children;

  VerifyOrExit((children = mle_->GetChildren(&num_children)) != NULL, ;);

  for (int i = 0; i < num_children; i++) {
    if (children[i].state == Neighbor::kStateInvalid)
      continue;
    snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x: ",
             children[i].mac_addr.bytes[0], children[i].mac_addr.bytes[1],
             children[i].mac_addr.bytes[2], children[i].mac_addr.bytes[3],
             children[i].mac_addr.bytes[4], children[i].mac_addr.bytes[5],
             children[i].mac_addr.bytes[6], children[i].mac_addr.bytes[7]);
    cur += strlen(cur);
    snprintf(cur, end - cur, "%04x, ", children[i].valid.address16);
    cur += strlen(cur);
    snprintf(cur, end - cur, "%d, ", children[i].state);
    cur += strlen(cur);
    if (children[i].mode & Mle::kModeRxOnWhenIdle) {
      snprintf(cur, end - cur, "r");
      cur += strlen(cur);
    }
    if (children[i].mode & Mle::kModeSecureDataRequest) {
      snprintf(cur, end - cur, "s");
      cur += strlen(cur);
    }
    if (children[i].mode & Mle::kModeFFD) {
      snprintf(cur, end - cur, "d");
      cur += strlen(cur);
    }
    if (children[i].mode & Mle::kModeFullNetworkData) {
      snprintf(cur, end - cur, "n");
      cur += strlen(cur);
    }
    snprintf(cur, end - cur, "\r\n");
    cur += strlen(cur);
    count++;
  }

exit:
  snprintf(cur, end - cur, "Total: %d\r\n", count);
  cur += strlen(cur);
  return cur - buf;
}

int CliThread::PrintKey(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  uint8_t key[16];
  uint8_t key_length;
  netif_->GetKeyManager()->GetMasterKey(key, &key_length);

  for (int i = 0; i < key_length; i++) {
    snprintf(cur, end - cur, "%02x", key[i]);
    cur += strlen(cur);
  }
  snprintf(cur, end - cur, "\r\n");
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintKeySequence(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  snprintf(cur, end - cur, "%d\r\n", netif_->GetKeyManager()->GetCurrentKeySequence());
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintLeaderData(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  Mle::LeaderData *leader_data;

  leader_data = mle_->GetLeaderData();
  snprintf(cur, end - cur, "partition_id = %08" PRIx32 "\r\n", leader_data->partition_id);
  cur += strlen(cur);
  snprintf(cur, end - cur, "weighting = %d\r\n", leader_data->weighting);
  cur += strlen(cur);
  snprintf(cur, end - cur, "version = %d\r\n", leader_data->version);
  cur += strlen(cur);
  snprintf(cur, end - cur, "stable_version = %d\r\n", leader_data->stable_version);
  cur += strlen(cur);
  snprintf(cur, end - cur, "leader_router_id = %d\r\n", leader_data->leader_router_id);
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintMode(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  uint8_t mode = mle_->GetDeviceMode();
  if (mode & Mle::kModeRxOnWhenIdle)
    *cur++ = 'r';
  if (mode & Mle::kModeSecureDataRequest)
    *cur++ = 's';
  if (mode & Mle::kModeFFD)
    *cur++ = 'd';
  if (mode & Mle::kModeFullNetworkData)
    *cur++ = 'n';
  snprintf(cur, end - cur, "\r\n");
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintRouters(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  int count = 0;
  Router *routers;
  uint8_t num_routers;

  VerifyOrExit((routers = mle_->GetRouters(&num_routers)) != NULL, ;);

  count = 0;
  for (int i = 0; i < num_routers; i++) {
    if (routers[i].state == Neighbor::kStateInvalid)
      continue;
    snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x: ",
             routers[i].mac_addr.bytes[0], routers[i].mac_addr.bytes[1],
             routers[i].mac_addr.bytes[2], routers[i].mac_addr.bytes[3],
             routers[i].mac_addr.bytes[4], routers[i].mac_addr.bytes[5],
             routers[i].mac_addr.bytes[6], routers[i].mac_addr.bytes[7]);
    cur += strlen(cur);
    snprintf(cur, end - cur, "%04x\r\n", routers[i].valid.address16);
    cur += strlen(cur);
    count++;
  }

exit:
  snprintf(cur, end - cur, "Total: %d\r\n", count);
  cur += strlen(cur);
  return cur - buf;
}

int CliThread::PrintRoutes(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  int count = 0;
  Router *routers;
  uint8_t num_routers;

  VerifyOrExit((routers = mle_->GetRouters(&num_routers)) != NULL, ;);

  snprintf(cur, end - cur, "seq: %d\r\n", mle_->GetRouterIdSequence());
  cur += strlen(cur);

  snprintf(cur, end - cur, "mask: ");
  cur += strlen(cur);
  for (int i = 0; i < Mle::kMaxRouterId; i++) {
    if (routers[i].allocated) {
      snprintf(cur, end - cur, "%d ", i);
      cur += strlen(cur);
    }
  }
  snprintf(cur, end - cur, "\r\n");
  cur += strlen(cur);

  count = 0;
  switch (mle_->GetDeviceState()) {
    case Mle::kDeviceStateDisabled:
    case Mle::kDeviceStateDetached:
      break;
    case Mle::kDeviceStateChild:
      snprintf(cur, end - cur, "%04x: %04x (0)\r\n",
               MacFrame::kShortAddrBroadcast, routers->valid.address16);
      cur += strlen(cur);
      count++;
      break;
    case Mle::kDeviceStateRouter:
    case Mle::kDeviceStateLeader:
      for (int i = 0; i < num_routers; i++) {
        if (routers[i].allocated == false)
          continue;
        snprintf(cur, end - cur, "%d: %d, %d, %d, %d\r\n",
                 i, routers[i].state, routers[i].nexthop, routers[i].cost,
                 (Timer::GetNow() - routers[i].last_heard) / 1000U);
        cur += strlen(cur);
        count++;
      }
      break;
  }

exit:
  snprintf(cur, end - cur, "Total: %d\r\n", count);
  cur += strlen(cur);
  return cur - buf;
}

int CliThread::PrintState(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  switch (mle_->GetDeviceState()) {
    case Mle::kDeviceStateDisabled:
      snprintf(cur, end - cur, "disabled\r\n");
      break;
    case Mle::kDeviceStateDetached:
      snprintf(cur, end - cur, "detached\r\n");
      break;
    case Mle::kDeviceStateChild:
      snprintf(cur, end - cur, "child\r\n");
      break;
    case Mle::kDeviceStateRouter:
      snprintf(cur, end - cur, "router\r\n");
      break;
    case Mle::kDeviceStateLeader:
      snprintf(cur, end - cur, "leader\r\n");
      break;
  }
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintTimeout(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  snprintf(cur, end - cur, "%d\n", mle_->GetTimeout());
  cur += strlen(cur);

  return cur - buf;
}

int CliThread::PrintWeight(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  snprintf(cur, end - cur, "%d\n", mle_->GetLeaderWeight());
  cur += strlen(cur);

  return cur - buf;
}

void CliThread::Run(int argc, char *argv[], CliServer *server) {
  char buf[512];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      goto print_usage;
    } else if (strcmp(argv[i], "cache") == 0) {
      cur += PrintAddressCache(buf, sizeof(buf));
      goto done;
    } else if (strcmp(argv[i], "children") == 0) {
      cur += PrintChildren(buf, sizeof(buf));
      goto done;
    } else if (strcmp(argv[i], "key") == 0) {
      if (++i >= argc) {
        cur += PrintKey(cur, sizeof(buf));
        goto done;
      }

      uint8_t key[16];
      int key_length;
      if ((key_length = hex2bin(argv[i], key, sizeof(key))) < 0)
        goto print_usage;
      netif_->GetKeyManager()->SetMasterKey(key, key_length);
      goto done;
    } else if (strcmp(argv[i], "key_sequence") == 0) {
      if (++i >= argc) {
        cur += PrintKeySequence(cur, sizeof(buf));
        goto done;
      }

      uint32_t key_sequence;
      key_sequence = strtol(argv[i], NULL, 0);
      netif_->GetKeyManager()->SetCurrentKeySequence(key_sequence);
      goto done;
    } else if (strcmp(argv[i], "leader_data") == 0) {
      cur += PrintLeaderData(buf, sizeof(buf));
      goto done;
    } else if (strcmp(argv[i], "mode") == 0) {
      if (++i >= argc) {
        cur += PrintMode(cur, sizeof(buf));
        goto done;
      }

      uint8_t mode = 0;
      for (char *arg = argv[i]; *arg != '\0'; arg++) {
        switch (*arg) {
          case 'r': mode |= Mle::kModeRxOnWhenIdle; break;
          case 's': mode |= Mle::kModeSecureDataRequest; break;
          case 'd': mode |= Mle::kModeFFD; break;
          case 'n': mode |= Mle::kModeFullNetworkData; break;
          default: goto print_usage;
        }
      }

      mle_->SetDeviceMode(mode);
      goto done;
    } else if (strcmp(argv[i], "network_id_timeout") == 0) {
      if (++i >= argc) {
        goto done;
      }

      uint32_t network_id_timeout;
      network_id_timeout = strtol(argv[i], NULL, 0);
      mle_->SetNetworkIdTimeout(network_id_timeout);
      goto done;
    } else if (strcmp(argv[i], "release_router") == 0) {
      if (++i >= argc) {
        goto done;
      }

      uint32_t router_id;
      router_id = strtol(argv[i], NULL, 0);
      mle_->ReleaseRouterId(router_id);
      goto done;
    } else if (strcmp(argv[i], "router_upgrade_threshold") == 0) {
      if (++i >= argc) {
        goto done;
      }

      uint32_t router_upgrade_threshold;
      router_upgrade_threshold = strtol(argv[i], NULL, 0);
      mle_->SetRouterUpgradeThreshold(router_upgrade_threshold);
      goto done;
    } else if (strcmp(argv[i], "routers") == 0) {
      cur += PrintRouters(buf, sizeof(buf));
      goto done;
    } else if (strcmp(argv[i], "routes") == 0) {
      cur += PrintRoutes(buf, sizeof(buf));
      goto done;
    } else if (strcmp(argv[i], "start") == 0) {
      netif_->Up();
      goto done;
    } else if (strcmp(argv[i], "state") == 0) {
      if (++i >= argc) {
        cur += PrintState(cur, sizeof(buf));
        goto done;
      } else if (strcmp(argv[i], "detached") == 0) {
        mle_->BecomeDetached();
        goto done;
      } else if (strcmp(argv[i], "child") == 0) {
        mle_->BecomeChild(Mle::kJoinSamePartition);
        goto done;
      } else if (strcmp(argv[i], "router") == 0) {
        mle_->BecomeRouter();
        goto done;
      } else if (strcmp(argv[i], "leader") == 0) {
        mle_->BecomeLeader();
        goto done;
      }
      goto print_usage;
    } else if (strcmp(argv[i], "stop") == 0) {
      netif_->Down();
      goto done;
    } else if (strcmp(argv[i], "timeout") == 0) {
      if (++i >= argc) {
        cur += PrintTimeout(buf, sizeof(buf));
        goto done;
      }

      uint32_t timeout;
      timeout = strtol(argv[i], NULL, 0);
      mle_->SetTimeout(timeout);
      goto done;
    } else if (strcmp(argv[i], "weight") == 0) {
      if (++i >= argc) {
        cur += PrintWeight(buf, sizeof(buf));
        goto done;
      }

      uint8_t weight;
      weight = strtol(argv[i], NULL, 0);
      mle_->SetLeaderWeight(weight);
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
