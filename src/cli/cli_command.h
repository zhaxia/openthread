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

#ifndef CLI_CLI_COMMAND_H_
#define CLI_CLI_COMMAND_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>

namespace Thread {

class CliCommand {
 public:
  explicit CliCommand(CliServer *cli_server);
  virtual const char *GetName() = 0;
  virtual void Run(int argc, char *argv[], class CliServer *server) = 0;

  CliCommand *next_ = NULL;
};

}  // namespace Thread

#endif  // CLI_CLI_COMMAND_H_
