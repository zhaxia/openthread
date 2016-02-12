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

#ifndef PLATFORM_KW2X_UART_H_
#define PLATFORM_KW2X_UART_H_

#include <common/tasklet.h>
#include <platform/common/uart_interface.h>

namespace Thread {

class Uart: public UartInterface {
 public:
  explicit Uart(Callbacks *callbacks);
  ThreadError Start() final;
  ThreadError Stop() final;

  ThreadError Send(const uint8_t *buf, uint16_t buf_len) final;
  void HandleIrq();

 private:
  static void ReceiveTask(void *context);
  void ReceiveTask();
  Tasklet receive_task_;
  static void SendTask(void *context);
  void SendTask();
  Tasklet send_task_;

  enum {
    kRxBufferSize = 128,
  };
  uint8_t rx_buffer_[kRxBufferSize];
  uint8_t rx_head_, rx_tail_;
};

}  // namespace Thread

#endif  // PLATFORM_KW2X_UART_H_
