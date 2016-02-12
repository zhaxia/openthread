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

#ifndef CLI_CLI_PHY_H_
#define CLI_CLI_PHY_H_

#include <cli/cli_command.h>
#include <thread/thread_netif.h>

namespace Thread {

class CliPhy: public CliCommand {
 public:
  explicit CliPhy(CliServer *server, ThreadNetif *netif);
  const char *GetName() final;
  void Run(int argc, char *argv[], CliServer *server) final;

 private:
  int PrintUsage(char *buf, uint16_t buf_length);

  int PhyTx(char *buf, uint16_t buf_length);

  Phy *phy_;
  PhyPacket packet_tx_;
  PhyPacket packet_rx_;
  ThreadNetif *netif_;

};

}  // namespace Thread

#endif  // CLI_CLI_PHY_H_
