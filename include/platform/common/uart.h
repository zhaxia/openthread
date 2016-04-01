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
 */

/**
 *    @file
 *    @brief
 *      Defines the interface to the Uart object that supports the
 *      Thread stack's serial communication.
 *
 *    @author    Jonathan Hui <jonhui@nestlabs.com>
 */

#ifndef UART_H_
#define UART_H_

#include <stdint.h>
#include <common/thread_error.h>

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the UART peripheral.
 *
 * @retval Thread::kThreadError_None Start was successful.
 * @retval Thread::kThreadError_Error Start was not successful.
 */
ThreadError uart_start();

/**
 * Stop the UART peripheral.
 *
 * @retval Thread::kThreadError_None Stop was successful.
 * @retval Thread::kThreadError_Error Stop was not successful.
 */
ThreadError uart_stop();

/**
 * Send bytes over the UART.
 *
 * @param[in] buf Pointer to the data buffer.
 * @param[in] buf_length Length of the data buffer.
 *
 * @retval Thread::kThreadError_None Send was successful.
 * @retval Thread::kThreadError_Error Send was not successful.
 */
ThreadError uart_send(const uint8_t *buf, uint16_t buf_length);

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread

#endif  // PLATFORM_COMMON_UART_INTERFACE_H_
