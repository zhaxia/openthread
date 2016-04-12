/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *    @brief
 *      Defines the interface to the Uart object that supports the
 *      Thread stack's serial communication.
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
