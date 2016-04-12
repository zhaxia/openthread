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

#ifndef PHY_H_
#define PHY_H_

#include <stdint.h>
#include <common/thread_error.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    kMaxPsduLength = 127,   ///< Maximum PSDU length
};

struct PhyPacket
{
    uint8_t m_length;
    uint8_t m_psdu[kMaxPsduLength];
    uint8_t m_channel;
    int8_t m_power;  // dBm
};

/**
 * The abstraction exposes the following transceiver states:
 */
enum PhyState
{
    /**
     * The transceiver is completely disabled and no configuration
     * parameters are retained.
     */
    kStateDisabled = 0,
    /**
     * The transceiver is in a sleep state and configuration
     * paraameters are retained.
     */
    kStateSleep = 1,
    /**
     * The receive and transmit paths are disabled but can transition
     * to Receive or Transmit states within 192us.
     */
    kStateIdle = 2,
    /**
     * The receive path is enabled and searching for preamble + SFD.
     */
    kStateListen = 3,
    /**
     * The receive path is enabled, a preamble and SFD was detected,
     * and a packet is being received.
     */
    kStateReceive = 4,
    /**
     * The transmit path is enabled.
     */
    kStateTransmit = 5,
};

/**
 * Intialize the transceiver.
 *
 * @return PhyInterface::kErrorNone if the transceiver initialized
 * successfully.
 */
ThreadError phy_init();

/**
 * Transitions the transceiver to Sleep from Disabled.  This call is
 * synchronous.
 *
 * @return PhyInterface::kErrorNone if the transceiver successfully
 * switches to Idle.
 * @return PhyInterface::kErrorInvalidState if the transceiver was
 * not in Disabled when called or if the transceiver could not be
 * switched to Idle.
 */
ThreadError phy_start();

/**
 * Transitions the transceiver to Disabled from any state.  This
 * call is synchronous.
 *
 * @return PhyInterface::kErrorNone if the transceiver successfully
 * switches to Disabled.
 * @return PhyInterface::kErrorInvalidState if the transceiver could
 * not be switched to Disabled.
 */
ThreadError phy_stop();

/**
 * Transitions the transceiver to Sleep from Idle.  This call is
 * synchronous.
 *
 * @return PhyInterface::kErrorNone if the transceiver successfully
 * switches to Sleep.
 * @return PhyInterface::kErrorInvalidState if the transceiver was
 * not in Idle when called or if the transceiver could not be
 * switched to Sleep.
 */
ThreadError phy_sleep();

/**
 * Transitions the transceiver to Idle from Sleep, Receive, or
 * Transmit.  This call is synchronous.
 *
 * @return PhyInterface::kErrorNone if the transceiver successfully
 * switches to Idle.
 * @return PhyInterface::kErrorInvalidState if the transceiver was
 * not in Sleep, Receive, or Transmit when called or if the
 * transceiver could not be switched to Idle.
 */
ThreadError phy_idle();

/**
 * Begins the receive sequence on the transeiver.  This call is
 * asynchronous.  The receive sequence consists of:
 * 1. Transitioning the transceiver to Receive from Idle
 * 2. Remain in Receive until a packet is received or reception is
 * aborted.
 * 3. Return to Idle.
 *
 * Upon completion of the receive sequence,
 * Callbacks::HandleReceiveDone() is called to signal completion to
 * the MAC layer.
 *
 * @param[in] channel the channel to use when receiving.  Valid values in the
 * range [11, 26].
 *
 * @return PhyInterface::kErrorNone if the transceiver successfully
 * switches to Receive.
 * @return PhyInterface::kErrorInvalidState if the transceiver was
 * not in Idle when called or if the transceiver could not be
 * switched to Receive.
 */
ThreadError phy_receive(PhyPacket *packet);

/**
 * Begins the transmit sequence on the transceiver.  This call is
 * asynchronous.  The transmit sequence consists of:
 * 1. Transitioning the transceiver to Transmit from Idle.
 * 2. Transmits the psdu on the given channel and at the given
 * transmit power.
 * 3. Return to Idle.
 *
 * Upon completion of the transmit sequence,
 * Callbacks::HandleTransmitDone is called to signal completion to
 * the MAC layer.
 *
 * @param[in] psdu the PSDU to transmit.
 * @param[in] psdu_length the length in bytes of the psdu.  Valid values
 * in the range [0, 127].
 * @param[in] channel the channel to use when transmitting.  Valid
 * values in the range [11, 26].
 * @param[in] power the transmit power in dBm to use when transmitting.
 *
 * @return PhyInterface::kErrorNone if the transceiver successfully
 * switches to Transmit.
 * @return PhyInterface::kErrorInvalidArgs: if any parameters are
 * not within valid range.
 * @return PhyInterface::kErrorInvalidState: if the transceiver was
 * not in Idle when called or if the transceiver could not be
 * switched to Receive.
 */
ThreadError phy_transmit(PhyPacket *packet);

/**
 * Gets the most recent noise floor measurement.  This call is
 * synchronous.
 *
 * @return The noise floor value in dBm when the noise floor value
 * is valid. 127 when noise floor value is invalid.
 */
int8_t phy_get_noise_floor();

/**
 * Get the current transciver state.  This call is synchronous.
 *
 * @return PhyInterface::kStateDisabled when in Disabled.
 * @return PhyInterface::kStateSleep when in Sleep.
 * @return PhyInterface::kStateIdle when in Idle.
 * @return PhyInterface::kStateListen when in Listen.
 * @return PhyInterface::kStateReceive when in Receive.
 * @return PhyInterface::kStateTransmit when in Transmit.
 */
PhyState phy_get_state();

ThreadError phy_set_pan_id(uint16_t panid);
ThreadError phy_set_extended_address(uint8_t *address);
ThreadError phy_set_short_address(uint16_t address);

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // PHY_H_
