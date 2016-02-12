/**
 * Internal command line interface (CLI) shell for 802.15.4 PHY.
 *
 *    @file    cli_phy.cc
 *    @author  Martin Turon <mturon@nestlabs.com>
 *    @date    August 7, 2015
 *
 *    Copyright (c) 2015 Nest Labs, Inc.  All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 */

#include <cli/cli.h>
#include <cli/cli_phy.h>
#include <common/code_utils.h>
#include <stdlib.h>

namespace Thread {

static const char kName[] = "phy";

static const char *phy_error_table[] = {
  "0 (ErrorNone)",
  "1 (ErrorInvalidArgs)",
  "2 (ErrorInvalidState)",
  "3 (ErrorAbort)"
};

static const char *phy_state_table[] = {
  "0 (StateDisabled)",
  "1 (StateSleep)",
  "2 (StateIdle)",
  "3 (StateListen)",
  "4 (StateReceive)",
  "5 (StateTransmit)"
};

CliPhy::CliPhy(CliServer *server, ThreadNetif *netif) : CliCommand(server) {
  phy_ = netif->GetMac()->GetPhy();
  netif_ = netif;
}

const char *CliPhy::GetName() {
  return kName;
}

int CliPhy::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "usage: phy\r\n"
           "  channel [channel]\r\n"
           "  power [power]\r\n"
           "  panid [panid]\r\n"
           "  addr16 [addr16]\r\n"
           "  state\r\n"
           "  noise\r\n"
           "  start\r\n"
           "  stop\r\n"
           "  sleep\r\n"
           "  idle\r\n"
           "  tx [length] [count]\r\n"
           "  rx\r\n"
	   );
  cur += strlen(cur);

  return cur - buf;
}

int CliPhy::PhyTx(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "sending packet\r\n"
	   );
  cur += strlen(cur);

  {
    int len = 0;
    uint8_t *payload = packet_tx_.GetPsdu();
    *payload++ = len++;
    *payload++ = len++;
    *payload++ = len++;
    *payload++ = len++;
    *payload++ = len++;
    *payload++ = len++;
    *payload++ = len++;
    *payload++ = len++;
    packet_tx_.SetPsduLength(len);
    packet_tx_.SetChannel(11);
    phy_->Transmit(&packet_tx_);
  }

  return cur - buf;
}

void CliPhy::Run(int argc, char *argv[], CliServer *server) {
  char buf[2048];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  for (int i = 0; i < argc; i++) {
    if ((strcmp(argv[i], "-h") == 0) ||
	(strcmp(argv[i], "-?") == 0)) {
      goto print_usage;

    } else if (strcmp(argv[i], "channel") == 0) {
      uint8_t channel;
      if (++i >= argc) {
	//channel = phy_->GetChannel();
        snprintf(cur, end - cur, "%d\r\n", channel);
        cur += strlen(cur);
        goto done;
      }

      channel = strtol(argv[i], NULL, 0);
      //phy_->SetChannel(channel);
      goto done;

    } else if (strcmp(argv[i], "power") == 0) {
      uint8_t power;
      if (++i >= argc) {
	//power = phy_->GetPower();
        snprintf(cur, end - cur, "%d\r\n", power);
        cur += strlen(cur);
        goto done;
      }

      power = strtol(argv[i], NULL, 0);
      //phy_->SetPower(power);
      goto done;

    } else if (strcmp(argv[i], "panid") == 0) {
      uint16_t panid;
      if (++i >= argc) {
	//panid = phy_->GetPanId();
        snprintf(cur, end - cur, "%d\r\n", panid);
        cur += strlen(cur);
        goto done;
      }

      panid = strtol(argv[i], NULL, 0);
      phy_->SetPanId(panid);
      goto done;

    } else if (strcmp(argv[i], "addr16") == 0) {
      uint16_t addr16;
      if (++i >= argc) {
	//addr16 = phy_->GetShortAddress();
        snprintf(cur, end - cur, "%d\r\n", addr16);
        cur += strlen(cur);
        goto done;
      }

      addr16 = strtol(argv[i], NULL, 0);
      phy_->SetShortAddress(addr16);
      goto done;

    } else if (strcmp(argv[i], "state") == 0) {
      uint8_t state;
      state = phy_->GetState();
      if (state < sizeof(phy_state_table))
	snprintf(cur, end - cur, "%s\r\n", phy_state_table[state]);
      cur += strlen(cur);
      goto done;

    } else if (strcmp(argv[i], "noise") == 0) {
      uint8_t noise;
      noise = phy_->GetNoiseFloor();
      snprintf(cur, end - cur, "%d\r\n", noise);
      cur += strlen(cur);
      goto done;

    } else if (strcmp(argv[i], "stop") == 0) {
      Phy::Error err = phy_->Stop();
      if (err < sizeof(phy_error_table))
	snprintf(cur, end - cur, "%s\r\n", phy_error_table[err]);
      cur += strlen(cur);
      goto done;

    } else if (strcmp(argv[i], "start") == 0) {
      Phy::Error err = phy_->Start();
      if (err < sizeof(phy_error_table))
	snprintf(cur, end - cur, "%s\r\n", phy_error_table[err]);
      cur += strlen(cur);
      goto done;

    } else if (strcmp(argv[i], "sleep") == 0) {
      Phy::Error err = phy_->Sleep();
      if (err < sizeof(phy_error_table))
	snprintf(cur, end - cur, "%s\r\n", phy_error_table[err]);
      cur += strlen(cur);
      goto done;

    } else if (strcmp(argv[i], "idle") == 0) {
      Phy::Error err = phy_->Idle();
      if (err < sizeof(phy_error_table))
	snprintf(cur, end - cur, "%s\r\n", phy_error_table[err]);
      cur += strlen(cur);
      goto done;

    } else if (strcmp(argv[i], "tx") == 0) {
      cur += PhyTx(cur, end - cur);
      goto done;

    } else if (strcmp(argv[i], "rx") == 0) {
      //phy_->Receive(&packet_rx_);
      goto done;
    }
  }

exit:
print_usage:
  cur += PrintUsage(cur, end - cur);

done:
  snprintf(cur, end - cur, "Done\r\n");
  cur += strlen(cur);
  server->Output(buf, cur - buf);
}

}  // namespace Thread
