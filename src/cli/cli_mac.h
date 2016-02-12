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

#ifndef CLI_CLI_MAC_H_
#define CLI_CLI_MAC_H_

#include <cli/cli_command.h>
#include <thread/thread_netif.h>

namespace Thread {

class CliMac: public CliCommand {
 public:
  explicit CliMac(CliServer *server, ThreadNetif *netif);
  const char *GetName() final;
  void Run(int argc, char *argv[], CliServer *server) final;

 private:
  static void HandleActiveScanResult(void *context, Mac::ActiveScanResult *result);
  void HandleActiveScanResult(Mac::ActiveScanResult *result);

  int PrintUsage(char *buf, uint16_t buf_length);
  int PrintWhitelist(char *buf, uint16_t buf_length);

  CliServer *server_;
  Mac *mac_;
  ThreadNetif *netif_;
};

}  // namespace Thread

#endif  // CLI_CLI_MAC_H_
