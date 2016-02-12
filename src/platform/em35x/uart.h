/**
 * Uart Adaptor to lib/Thread tasklet context from CpuUart driver abstraction.
 *
 *    @file    da15100/uart.cc
 *    @date    2015/11/12
 *
 *    @author  WenZheng Li  <wenzheng@nestlabs.com>
 *    @author  Martin Turon <mturon@nestlabs.com>
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
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
 */

#ifndef PLATFORM_EM35X_UART_H_
#define PLATFORM_EM35X_UART_H_

#include <common/tasklet.h>
#include <platform/common/uart_interface.h>

namespace Thread {

class Uart: public UartInterface {
 public:
  explicit Uart(Callbacks *callbacks);
  ThreadError Start() final;
  ThreadError Stop() final;

  ThreadError Send(const uint8_t *buf, uint16_t buf_len) final;
};

}  // namespace Thread

#endif  // PLATFORM_EM35X_UART_H_
