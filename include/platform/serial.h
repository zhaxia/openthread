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
 * @file
 * @brief
 *   This file includes the platform abstraction for serial communication.
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
 * @brief
 *   This module includes the platform abstraction for serial communication.
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
ThreadError otSerialEnable(void);

/**
 * Disable the serial.
 *
 * @retval ::kThreadError_None  Successfully disabled the serial.
 * @retval ::kThreadError_Fail  Failed to disable the serial.
 */
ThreadError otSerialDisable(void);

/**
 * Send bytes over the serial.
 *
 * @param[in] aBuf        A pointer to the data buffer.
 * @param[in] aBufLength  Number of bytes to transmit.
 *
 * @retval ::kThreadError_None  Successfully started transmission.
 * @retval ::kThreadError_Fail  Failed to start the transmission.
 */
ThreadError otSerialSend(const uint8_t *aBuf, uint16_t aBufLength);

/**
 * Signal that the bytes send operation has completed.
 *
 * This may be called from interrupt context.  This will schedule calls to otSerialHandleSendDone().
 */
extern void otSerialSignalSendDone(void);

/**
 * Complete the send sequence.
 */
void otSerialHandleSendDone(void);

/**
 * Signal that bytes have been received.
 *
 * This may be called from interrupt context.  This will schedule calls to otSerialGetReceivedBytes() and
 * otSerialHandleReceiveDone().
 */
extern void otSerialSignalReceive(void);

/**
 * Get a pointer to the received bytes.
 *
 * @param[out]  aBufLength  A pointer to a variable that this function will put the number of bytes received.
 *
 * @returns A pointer to the received bytes.  NULL, if there are no received bytes to process.
 */
const uint8_t *otSerialGetReceivedBytes(uint16_t *aBufLength);

/**
 * Release received bytes.
 */
void otSerialHandleReceiveDone();

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SERIAL_H_
