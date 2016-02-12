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

#include <platform/kw2x/uart.h>
#include <cpu/CpuGpio.hpp>

using nl::Thread::CpuGpio;

namespace Thread {

#define PLATFORM_CLOCK  20971520
#define UART1_IRQ_NUM   33

static Uart *uart_ = NULL;

Uart::Uart(Callbacks *callbacks) :
    UartInterface(callbacks),
    receive_task_(&ReceiveTask, this),
    send_task_(&SendTask, this) {
}

ThreadError Uart::Start() {
  uint32_t baud = 115200;
  UART_MemMapPtr uart = UART1_BASE_PTR;
  uint32_t div = (2 * PLATFORM_CLOCK) / baud;
  uint16_t bd = div / 32;

  uart_ = this;
  rx_head_ = 0;
  rx_tail_ = 0;

  SIM_BASE_PTR->SCGC4 |= SIM_SCGC4_UART1_MASK;

  uart->C2 = 0;

  uart->BDH = bd >> 8;
  uart->BDL = bd & 0xff;
  uart->C4 = bd % 32;

  uart->C1 = 0;

  CpuGpio::config(4, 0, 3);
  CpuGpio::config(4, 1, 3);
  CpuGpio::config(4, 2, 3);
  CpuGpio::config(4, 3, 3);

  uart->C2 |= UART_C2_RE_MASK | UART_C2_TE_MASK | UART_C2_RIE_MASK;

  NVIC_EnableIRQ(UART1_IRQ_NUM);

  return kThreadError_None;
}

ThreadError Uart::Stop() {
  // XXX: Not implemented
  return kThreadError_Error;
}

ThreadError Uart::Send(const uint8_t *buf, uint16_t buf_len) {
  UART_MemMapPtr uart = UART1_BASE_PTR;

  for ( ; buf_len > 0; buf_len--) {
    while (!(uart->S1 & UART_S1_TDRE_MASK)) {}
    uart->D = *buf++;
  }

  send_task_.Post();

  return kThreadError_None;
}

void Uart::SendTask(void *context) {
  Uart *obj = reinterpret_cast<Uart*>(context);
  obj->SendTask();
}

void Uart::SendTask() {
  callbacks_->HandleSendDone();
}

extern "C" void UART1_IrqHandler() {
  if (uart_ != NULL)
    uart_->HandleIrq();
}

void Uart::HandleIrq() {
  UART_MemMapPtr uart = UART1_BASE_PTR;
  volatile uint8_t s, b;

  s = uart->S1;
  b = uart->D;

  uint8_t new_tail = (rx_tail_ + 1) % kRxBufferSize;
  if (new_tail != rx_head_) {
    rx_buffer_[rx_tail_] = b;
    rx_tail_ = new_tail;
  }

  receive_task_.Post();
}

void Uart::ReceiveTask(void *context) {
  Uart *obj = reinterpret_cast<Uart*>(context);
  obj->ReceiveTask();
}

void Uart::ReceiveTask() {
  if (rx_head_ > rx_tail_) {
    callbacks_->HandleReceive(rx_buffer_ + rx_head_, kRxBufferSize - rx_head_);
    rx_head_ = 0;
  }

  if (rx_head_ < rx_tail_) {
    callbacks_->HandleReceive(rx_buffer_ + rx_head_, rx_tail_ - rx_head_);
    rx_head_ = rx_tail_;
  }
}

}  // namespace Thread
