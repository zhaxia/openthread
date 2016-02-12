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

#ifndef CLI_CLI_SERIAL_H_
#define CLI_CLI_SERIAL_H_

#include <cli/cli_server.h>
#include <common/thread_error.h>
#include <common/message.h>
#include <platform/common/uart.h>

namespace Thread {

class CliCommand;

class CliServerSerial: public CliServer, private UartInterface::Callbacks {
 public:
  CliServerSerial();
  ThreadError Add(CliCommand *command) final;
  ThreadError Start(uint16_t port) final;
  ThreadError Output(const char *buf, uint16_t buf_length) final;

 private:
  enum {
    kMaxArgs = 8,
    kRxBufferSize = 128,
  };

  void HandleReceive(uint8_t *buf, uint16_t buf_length) final;
  void HandleSendDone() final;
  ThreadError ProcessCommand();

  CliCommand *commands_ = NULL;
  char rx_buffer_[kRxBufferSize];
  uint16_t rx_length_;
  Uart uart_;
};

}  // namespace Thread

#endif  // CLI_CLI_SERIAL_H_
