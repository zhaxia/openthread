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

#ifndef PLATFORM_POSIX_UART_H_
#define PLATFORM_POSIX_UART_H_

#include <common/tasklet.h>
#include <platform/common/uart_interface.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

namespace Thread {

class Uart: public UartInterface {
 public:
  explicit Uart(Callbacks *callbacks);
  ThreadError Start() final;
  ThreadError Stop() final;

  ThreadError Send(const uint8_t *buf, uint16_t buf_length) final;

 private:
  static void ReceiveTask(void *context);
  void ReceiveTask();
  static void SendTask(void *context);
  void SendTask();
  static void *ReceiveThread(void *arg);

  Tasklet receive_task_;
  Tasklet send_task_;

  int fd_;
  pthread_t thread_;
  sem_t *semaphore_;
};

}  // namespace Thread

#endif  // PLATFORM_POSIX_UART_H_
