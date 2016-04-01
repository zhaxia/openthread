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

#include <platform/common/uart.h>
#include <common/tasklet.h>
#include <cpu/CpuGpio.hpp>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    kRxBufferSize = 128,
    kPlatformClock = 20971520,
    kUart1IrqNum = 33,
    kBaudRate = 115200,
};

extern void uart_handle_receive(uint8_t *buf, uint16_t buf_length);
extern void uart_handle_send_done();

static void receive_task(void *context);
static void send_task(void *context);

static Thread::Tasklet s_receive_task(&receive_task, NULL);
static Thread::Tasklet s_send_task(&send_task, NULL);

static uint8_t s_rx_buffer[kRxBufferSize];
static uint8_t s_rx_head, s_rx_tail;

ThreadError uart_start()
{
    uint32_t baud = kBaudRate;
    UART_MemMapPtr uart = UART1_BASE_PTR;
    uint32_t div = (2 * kPlatformClock) / baud;
    uint16_t bd = div / 32;

    s_rx_head = 0;
    s_rx_tail = 0;

    SIM_BASE_PTR->SCGC4 |= SIM_SCGC4_UART1_MASK;

    uart->C2 = 0;

    uart->BDH = bd >> 8;
    uart->BDL = bd & 0xff;
    uart->C4 = bd % 32;

    uart->C1 = 0;

    nl::Thread::CpuGpio::config(4, 0, 3);
    nl::Thread::CpuGpio::config(4, 1, 3);
    nl::Thread::CpuGpio::config(4, 2, 3);
    nl::Thread::CpuGpio::config(4, 3, 3);

    uart->C2 |= UART_C2_RE_MASK | UART_C2_TE_MASK | UART_C2_RIE_MASK;

    NVIC_EnableIRQ(kUart1IrqNum);

    return kThreadError_None;
}

ThreadError uart_stop()
{
    // XXX: Not implemented
    return kThreadError_Error;
}

ThreadError uart_send(const uint8_t *buf, uint16_t buf_len)
{
    UART_MemMapPtr uart = UART1_BASE_PTR;

    for (; buf_len > 0; buf_len--)
    {
        while (!(uart->S1 & UART_S1_TDRE_MASK)) {}

        uart->D = *buf++;
    }

    s_send_task.Post();

    return kThreadError_None;
}

void send_task(void *context)
{
    uart_handle_send_done();
}

extern "C" void UART1_IrqHandler()
{
    UART_MemMapPtr uart = UART1_BASE_PTR;
    uint8_t new_tail;
    volatile uint8_t s, b;

    s = uart->S1;
    b = uart->D;

    new_tail = (s_rx_tail + 1) % kRxBufferSize;

    if (new_tail != s_rx_head)
    {
        s_rx_buffer[s_rx_tail] = b;
        s_rx_tail = new_tail;
    }

    s_receive_task.Post();
}

void receive_task(void *context)
{
    if (s_rx_head > s_rx_tail)
    {
        uart_handle_receive(s_rx_buffer + s_rx_head, kRxBufferSize - s_rx_head);
        s_rx_head = 0;
    }

    if (s_rx_head < s_rx_tail)
    {
        uart_handle_receive(s_rx_buffer + s_rx_head, s_rx_tail - s_rx_head);
        s_rx_head = s_rx_tail;
    }
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
