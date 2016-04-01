/**
 * Implementation of the Uart Adaptor for lib/Thread tasklet context
 * to zircon CpuUart driver abstraction.
 *
 *    @file    da15100/uart.cc
 *    @date    2015/8/4
 *
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

#include <common/tasklet.h>
#include <core/cpu.hpp>
#include <os/ITask.hpp>
#include <io/IStreamAsync.hpp>

#include <platform/common/uart.h>

#define UART_BAUD              115200

using nl::Thread::CpuUart;
using nl::Thread::ITask;
using nl::Thread::IStreamAsync;

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

extern void uart_handle_receive(uint8_t *buf, uint16_t buf_length);

/// TODO: Use libhw abstraction.
/// This should call theHw.getUart() to get the resource,
/// not allocate the debug uart here for itself...
static CpuUart the_uart_(CPU_UART_DEFAULT);


/**
 * Callback handler for asynchronous uart driver.
 */
class CpuUartAsync : public IStreamAsync
{
    uint8_t rx_byte_;
    Tasklet task_;

public:
    CpuUartAsync(): task_(&RunTask, this) {}

    void init() {
        readKick();
    }

    /// Callback for the last read operation completing.
    virtual void readDone(uint8_t *buf, int len) {
        // TODO: Move to use lib/util/ring
        rx_byte_ = *buf;
        task_.Post();
    }

    /// Kick off a read of the next uart byte.
    void readKick() {
        the_uart_.readByte(rx_byte_);  // Kick next byte
    }

    /// Unused callback.  Currently, write is a blocking call.
    virtual void writeDone() {}


    /// Receive tasklet handler.
    static void RunTask(void *context) {
        CpuUartAsync *obj = (CpuUartAsync *)context;
        obj->RunTask();
    }

    void RunTask() {
        // TODO: Move to lib/util/ring
        uart_handle_receive(&rx_byte_, 1);
        readKick();
    }
};

static CpuUartAsync the_uart_async_;

ThreadError uart_start()
{
    the_uart_.init(UART_BAUD, &the_uart_async_);
    the_uart_async_.init();
    return kThreadError_None;
}

ThreadError uart_stop()
{
    // XXX: Not implemented
    return kThreadError_Error;
}

ThreadError uart_send(const uint8_t *buf, uint16_t buf_len)
{
    // Blocking write.
    // TODO: split into new IStreamAsync API.
    the_uart_.write((uint8_t *)buf, buf_len);

    return kThreadError_None;
}

#ifdef __cplusplus
}
#endif

}  // namespace Thread
