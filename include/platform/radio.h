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
 *   This file defines the radio interface for OpenThread.
 *
 */

#ifndef RADIO_HPP_
#define RADIO_HPP_

#include <stdint.h>

#include <common/thread_error.hpp>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup radio Radio
 * @ingroup platform
 *
 * @{
 *
 */

/**
 * @defgroup radio-types Types
 *
 * @{
 *
 */

enum
{
    kMaxPsduLength = 127,           ///< Maximum PSDU length
};

/**
 * This structure represents an IEEE 802.15.4 radio frame.
 */
typedef struct RadioPacket
{
    uint8_t mLength;                ///< Length of the PSDU.
    uint8_t mPsdu[kMaxPsduLength];  ///< The PSDU.
    uint8_t mChannel;               ///< Channel used to transmit/receive the frame.
    int8_t  mPower;                 ///< Transmit/receive power in dBm.
} RadioPacket;

/**
 * @}
 *
 */

/**
 * @defgroup radio-config Configuration
 *
 * @{
 *
 */

/**
 * Set the PAN ID for address filtering.
 *
 * @param[in] aPanId  The IEEE 802.15.4 PAN ID.
 *
 * @retval ::kThreadError_None  If the PAN ID was set properly.
 * @retval ::kThreadError_Fail  If the PAN ID was not set properly.
 */
ThreadError ot_radio_set_pan_id(uint16_t aPanId);

/**
 * Set the Extended Address for address filtering.
 *
 * @param[in] aExtendedAddress  A pointer to the IEEE 802.15.4 Extended Address.
 *
 * @retval ::kThreadError_None  If the Extended Address was set properly.
 * @retval ::kThreadError_Fail  If the Extended Address was not set properly.
 */
ThreadError ot_radio_set_extended_address(uint8_t *aExtendedAddress);

/**
 * Set the Short Address for address filtering.
 *
 * @param[in] aShortAddress  The IEEE 802.15.4 Short Address.
 *
 * @retval ::kThreadError_None  If the Short Address was set properly.
 * @retval ::kThreadError_Fail  If the Short Address was not set properly.
 */
ThreadError ot_radio_set_short_address(uint16_t aShortAddress);

/**
 * @}
 *
 */

/**
 * @defgroup radio-operation Operation
 *
 * @{
 *
 */

/**
 * Intialize the radio.
 */
void ot_radio_init();

/**
 * Enable the radio.
 *
 * @retval ::kThreadError_None  Successfully transitioned to Idle.
 * @retval ::kThreadError_Fail  Failed to transition to Idle.
 */
ThreadError ot_radio_enable();

/**
 * Disable the radio.
 *
 * @retval ::kThreadError_None  Successfully transitioned to Disabled.
 * @retval ::kThreadError_Fail  Failed to transition to Disabled.
 */
ThreadError ot_radio_disable();

/**
 * Transition the radio to Sleep.
 *
 * @retval ::kThreadError_None  Successfully transitioned to Sleep.
 * @retval ::kThreadError_Fail  Failed to transition to Sleep.
 */
ThreadError ot_radio_sleep();

/**
 * Transition the radio to Idle.
 *
 * @retval ::kThreadError_None  Successfully transitioned to Idle.
 * @retval ::kThreadError_Fail  Failed to transition to Idle.
 */
ThreadError ot_radio_idle();

/**
 * Begins the receive sequence on the radio.
 *
 * The receive sequence consists of:
 * 1. Transitioning the radio to Receive from Idle.
 * 2. Remain in Receive until a packet is received or reception is aborted.
 * 3. Return to Idle.
 *
 * Upon completion of the receive sequence, ot_radio_signal_receive_done() is called to signal completion to the MAC layer.
 *
 * @param[in]  aPacket  A pointer to a packet buffer.
 *
 * @note The channel is specified in @p aPacket.
 *
 * @retval ::kThreadError_None  Successfully transitioned to Receive.
 * @retval ::kThreadError_Fail  Failed to transition to Receive.
 */
ThreadError ot_radio_receive(RadioPacket *aPacket);

/**
 * Signal that a packet has been received.
 *
 * This may be called from interrupt context.  The MAC layer will then schedule a call to @fn ot_radio_handle_receive.
 */

extern void ot_radio_signal_receive_done();

/**
 * Complete the receive sequence.
 *
 * @retval ::kThreadError_None          Successfully received a frame.
 * @retval ::kThreadError_Abort         Reception was aborted and a frame was not received.
 * @retval ::kThreadError_InvalidState  The radio was not in Receive.
 */
ThreadError ot_radio_handle_receive_done();

/**
 * Begins the transmit sequence on the radio.
 *
 * The transmit sequence consists of:
 * 1. Transitioning the radio to Transmit from Idle.
 * 2. Transmits the psdu on the given channel and at the given transmit power.
 * 3. Return to Idle.
 *
 * Upon completion of the transmit sequence, ot_radio_signal_transmit_done() is called to signal completion to the MAC
 * layer.
 *
 * @param[in]  aPacket  A pointer to a packet buffer.
 *
 * @note The channel is specified in @p aPacket.
 * @note The transmit power is specified in @p aPacket.
 *
 * @retval ::kThreadError_None         Successfully transitioned to Transmit.
 * @retval ::kThreadError_InvalidArgs  One or more parameters in @p aPacket are invalid.
 * @retval ::kThreadError_Fail         Failed to transition to Transmit.
 */
ThreadError ot_radio_transmit(RadioPacket *aPacket);

/**
 * Signal that the requested transmission is complete.
 *
 * This may be called from interrupt context.  OpenThread will then schedule a call to
 *  @fn ot_radio_handle_transmit_done.
 */
extern void ot_radio_signal_transmit_done();

/**
 * Complete the transmit sequence on the radio.
 *
 * @param[out]  aFramePending  TRUE if an ACK frame was received and the Frame Pending bit was set.
 *
 * @retval ::kThreadError_None          The frame was transmitted.
 * @retval ::kThreadError_NoAck         The frame was transmitted, but no ACK was received.
 * @retval ::kThreadError_CcaFailed     The transmission was aborted due to CCA failure.
 * @retval ::kThreadError_Abort         The transmission was aborted for other reasons.
 * @retval ::kThreadError_InvalidState  The radio did not transmit a packet.
 */
ThreadError ot_radio_handle_transmit_done(bool *aFramePending);

/**
 * Get the most recent RSSI measurement.
 *
 * @returns The noise floor value in dBm when the noise floor value is valid.  127 when noise floor value is invalid.
 */
int8_t ot_radio_get_noise_floor();

/**
 * @}
 *
 */

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // RADIO_HPP_
