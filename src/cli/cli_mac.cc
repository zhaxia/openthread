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
#include <cli/cli_mac.h>
#include <common/code_utils.h>
#include <stdlib.h>

namespace Thread {

static const char kName[] = "mac";

CliMac::CliMac(CliServer *server, ThreadNetif *netif) : CliCommand(server) {
  mac_ = netif->GetMac();
  netif_ = netif;
  server_ = server;
}

const char *CliMac::GetName() {
  return kName;
}

int CliMac::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "usage: mac\r\n"
           "  addr16\r\n"
           "  addr64\r\n"
           "  channel [channel]\r\n"
           "  name [name]\r\n"
           "  panid [panid]\r\n"
           "  xpanid [xpanid]\r\n"
           "  scan [results]\r\n"
           "  whitelist [add|disable|enable|remove]\r\n");
  cur += strlen(cur);

  return cur - buf;
}

int CliMac::PrintWhitelist(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  MacWhitelist *whitelist = mac_->GetWhitelist();
  int length = whitelist->GetMaxEntries();

  if (whitelist->IsEnabled())
    snprintf(cur, end - cur, "Enabled\r\n");
  else
    snprintf(cur, end - cur, "Disabled\r\n");
  cur += strlen(cur);

  for (int i = 0; i < length; i++) {
    const uint8_t *addr = whitelist->GetAddress(i);
    if (addr == NULL)
      continue;
    snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
             addr[0], addr[1], addr[2], addr[3],
             addr[4], addr[5], addr[6], addr[7]);
    cur += strlen(cur);
  }

  return cur - buf;
}

void CliMac::Run(int argc, char *argv[], CliServer *server) {
  char buf[2048];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      goto print_usage;
    } else if (strcmp(argv[i], "addr16") == 0) {
      MacAddr16 addr16 = mac_->GetAddress16();
      snprintf(cur, end - cur, "%04x\r\n", addr16);
      cur += strlen(cur);
      goto done;
    } else if (strcmp(argv[i], "addr64") == 0) {
      const MacAddr64 *addr64 = mac_->GetAddress64();
      snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
               addr64->bytes[0], addr64->bytes[1], addr64->bytes[2], addr64->bytes[3],
               addr64->bytes[4], addr64->bytes[5], addr64->bytes[6], addr64->bytes[7]);
      cur += strlen(cur);
      goto done;
    } else if (strcmp(argv[i], "channel") == 0) {
      if (++i >= argc) {
        uint8_t channel = mac_->GetChannel();
        snprintf(cur, end - cur, "%d\r\n", channel);
        cur += strlen(cur);
        goto done;
      }

      uint8_t channel;
      channel = strtol(argv[i], NULL, 0);
      mac_->SetChannel(channel);
      goto done;
    } else if (strcmp(argv[i], "name") == 0) {
      if (++i >= argc) {
        const char *name = mac_->GetNetworkName();
        snprintf(cur, end - cur, "%s\r\n", name);
        cur += strlen(cur);
        goto done;
      }

      mac_->SetNetworkName(argv[i]);
      goto done;
    } else if (strcmp(argv[i], "panid") == 0) {
      if (++i >= argc) {
        uint16_t panid = mac_->GetPanId();
        snprintf(cur, end - cur, "%04x\r\n", panid);
        cur += strlen(cur);
        goto done;
      }

      uint16_t panid;
      panid = strtol(argv[i], NULL, 0);
      mac_->SetPanId(panid);
      goto done;
    } else if (strcmp(argv[i], "scan") == 0) {
      mac_->ActiveScan(kMacScanDefaultInterval, kMacScanChannelMaskAllChannels, HandleActiveScanResult, this);
      snprintf(cur, end - cur, "| Network Name     | Extended PAN     | PAN  | MAC Address      | Ch | dBm |\r\n");
      cur += strlen(cur);
      goto scan_wait;
    } else if (strcmp(argv[i], "whitelist") == 0) {
      if (++i >= argc) {
        cur += PrintWhitelist(cur, end - cur);
        goto done;
      } else if (strcmp(argv[i], "add") == 0) {
        VerifyOrExit(++i < argc, ;);

        MacAddr64 macaddr;
        VerifyOrExit(hex2bin(argv[i], macaddr.bytes, sizeof(macaddr.bytes)) == sizeof(macaddr.bytes), ;);

        int entry;
        VerifyOrExit((entry = mac_->GetWhitelist()->Add(&macaddr)) >= 0, ;);

        if (++i < argc) {
          int8_t rssi;
          rssi = strtol(argv[i], NULL, 0);
          mac_->GetWhitelist()->SetRssi(entry, rssi);
        }

        goto done;
      } else if (strcmp(argv[i], "clear") == 0) {
        mac_->GetWhitelist()->Clear();
        goto done;
      } else if (strcmp(argv[i], "disable") == 0) {
        mac_->GetWhitelist()->Disable();
        goto done;
      } else if (strcmp(argv[i], "enable") == 0) {
        mac_->GetWhitelist()->Enable();
        goto done;
      } else if (strcmp(argv[i], "remove") == 0) {
        VerifyOrExit(++i < argc, ;);

        MacAddr64 macaddr;
        VerifyOrExit(hex2bin(argv[i], macaddr.bytes, sizeof(macaddr.bytes)) == sizeof(macaddr.bytes), ;);
        mac_->GetWhitelist()->Remove(&macaddr);
        goto done;
      }
    } else if (strcmp(argv[i], "xpanid") == 0) {
      if (++i >= argc) {
        const uint8_t *xpanid = mac_->GetExtendedPanId();
        snprintf(cur, end - cur, "%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
                 xpanid[0], xpanid[1], xpanid[2], xpanid[3], xpanid[4], xpanid[5], xpanid[6], xpanid[7]);
        cur += strlen(cur);
        goto done;
      }

      uint8_t xpanid[8];
      memset(xpanid, 0, sizeof(xpanid));
      if (hex2bin(argv[i], xpanid, sizeof(xpanid)) < 0)
        goto print_usage;
      mac_->SetExtendedPanId(xpanid);
      goto done;
    }
  }

exit:
print_usage:
  cur += PrintUsage(cur, end - cur);

done:
  snprintf(cur, end - cur, "Done\r\n");
  cur += strlen(cur);

scan_wait:
  server->Output(buf, cur - buf);
}

void CliMac::HandleActiveScanResult(void *context, Mac::ActiveScanResult *result) {
  CliMac *obj = reinterpret_cast<CliMac*>(context);
  obj->HandleActiveScanResult(result);
}

void CliMac::HandleActiveScanResult(Mac::ActiveScanResult *result) {
  char buf[256];
  char *cur = buf;
  char *end = cur + sizeof(buf);
  uint8_t *bytes;

  if (result != NULL) {
    snprintf(cur, end - cur, "| %-16s ", result->network_name);
    cur += strlen(cur);

    bytes = result->ext_panid;
    snprintf(cur, end - cur, "| %02x%02x%02x%02x%02x%02x%02x%02x ",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
    cur += strlen(cur);

    snprintf(cur, end - cur, "| %04x ", result->panid);
    cur += strlen(cur);

    bytes = result->ext_addr;
    snprintf(cur, end - cur, "| %02x%02x%02x%02x%02x%02x%02x%02x ",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
    cur += strlen(cur);

    snprintf(cur, end - cur, "| %02d ", result->channel);
    cur += strlen(cur);

    snprintf(cur, end - cur, "| %03d ", result->rssi);
    cur += strlen(cur);

    snprintf(cur, end - cur, "|\r\n");
    cur += strlen(cur);
  } else {
    snprintf(cur, end - cur, "Done\r\n");
    cur += strlen(cur);
  }

  server_->Output(buf, cur - buf);
}

}  // namespace Thread
