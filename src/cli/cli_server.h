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

#ifndef CLI_CLI_SERVER_H_
#define CLI_CLI_SERVER_H_

#include <common/thread_error.h>
#include <common/message.h>

namespace Thread {

class CliCommand;

class CliServer {
 public:
  virtual ThreadError Add(CliCommand *command) = 0;
  virtual ThreadError Start(uint16_t port) = 0;
  virtual ThreadError Output(const char *buf, uint16_t buf_length) = 0;

 protected:
  enum {
    kMaxArgs = 8,
  };
  CliCommand *commands_ = NULL;
};

}  // namespace Thread

#endif  // CLI_CLI_SERVER_H_
