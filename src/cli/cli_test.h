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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#ifndef CLI_CLI_TEST_H_
#define CLI_CLI_TEST_H_

#include <cli/cli_command.h>
#include <common/timer.h>

namespace Thread {

class CliTest: public CliCommand {
 public:
  explicit CliTest(CliServer *server);
  const char *GetName() final;
  void Run(int argc, char *argv[], CliServer *server) final;

 private:
  int PrintUsage(char *buf, uint16_t buf_length);

  int TestTimer(char *buf, uint16_t buf_length);
  int TestPhyTx(char *buf, uint16_t buf_length);

  static void HandleTimer(void *context);
  void HandleTimer();
  Timer timer_;
};

}  // namespace Thread

#endif  // CLI_CLI_TEST_H_
