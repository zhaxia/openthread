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
 *      Defines the interface to the Uart object that supports the Thread stack's serial communication.
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdint.h>
#include <common/thread_error.hpp>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup serial Serial
 * @ingroup platform
 *
 * @{
 *
 */

/**
 * Enable the serial.
 *
 * @retval ::kThreadError_None  Successfully enabled the serial.
 * @retval ::kThreadError_Fail  Failed to enabled the serial.
 */
ThreadError ot_serial_enable(void);

/**
 * Disable the serial.
 *
 * @retval ::kThreadError_None  Successfully disabled the serial.
 * @retval ::kThreadError_Fail  Failed to disable the serial.
 */
ThreadError ot_serial_disable(void);

/**
 * Send bytes over the serial.
 *
 * @param[in] aBuf        A pointer to the data buffer.
 * @param[in] aBufLength  Number of bytes to transmit.
 *
 * @retval ::kThreadError_None  Successfully started transmission.
 * @retval ::kThreadError_Fail  Failed to start the transmission.
 */
ThreadError ot_serial_send(const uint8_t *aBuf, uint16_t aBufLength);

/**
 * Signal that the bytes send operation has completed.
 *
 * This may be called from interrupt context.  The will schedule a call to @fn ot_serial_handle_receive.
 */
extern void ot_serial_signal_send_done(void);

/**
 * Complete the send sequence.
 */
void ot_serial_handle_send_done(void);

/**
 * Signal that bytes have been received.
 *
 * This may be called from interrupt context.
 */
extern void ot_serial_signal_receive(void);

/**
 * Get a pointer to the received bytes.
 *
 * @param[out]  aBufLength  A pointer to a variable that this function will put the number of bytes received.
 *
 * @returns A pointer to the received bytes.  NULL, if there are no received bytes to process.
 */
const uint8_t *ot_serial_get_received_bytes(uint16_t *aBufLength);

/**
 * Release received bytes.
 */
void ot_serial_handle_receive_done();

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SERIAL_H_
