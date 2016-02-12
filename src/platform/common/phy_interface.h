/**
 * @file
 * @author    Jonathan Hui <jonhui@nestlabs.com>

 * @copyright Copyright (c) 2015 Nest Labs, Inc.  All rights reserved.
 *
 * @copyright This document is the property of Nest.  It is considered
 * confidential and proprietary information.
 *
 * @copyright This document may not be reproduced or transmitted in
 * any form, in whole or in part, without the express written
 * permission of Nest.
 */

#ifndef PLATFORM_COMMON_PHY_INTERFACE_H_
#define PLATFORM_COMMON_PHY_INTERFACE_H_

#include <stdint.h>

namespace Thread {

/**
 * A platform-indepdent abstraction of a PHY packet.
 *
 * A platform-specific PHY driver implements a PhyPacket class that
 * inherets the PhyPacketInterface.
 */
class PhyPacketInterface {
 public:
  enum {
    kMaxPsduLength = 127,   ///< Maximum PSDU length
  };

  /**
   * Get the PSDU length.
   *
   * @return the PSDU length.
   */
  virtual uint8_t GetPsduLength() const = 0;

  /**
   * Set the PSDU length.
   *
   * @param psdu_length the PSDU length.
   */
  virtual void SetPsduLength(uint8_t psdu_length) = 0;

  /**
   * Get a pointer to the PSDU.
   *
   * @return a pointer to the PSDU.
   */
  virtual uint8_t *GetPsdu() = 0;

  /**
   * Get the channel parameter.
   * * On reception, the channel is used to indicate what channel was
   *   used for reception.
   * * On transmission, the channel is used to indicate what channel
   *   to use for transmission.
   *
   * @return the channel.
   */
  virtual uint8_t GetChannel() const = 0;

  /**
   * Set the channel parameter.
   * * On reception, the channel is used to indicate what channel was
   *   used for reception.
   * * On transmission, the channel is used to indicate what channel
   *   to use for transmission.
   *
   * @param[in] channel the channel.
   */
  virtual void SetChannel(uint8_t channel) = 0;

  /**
   * Get the power parameter.
   * * On reception, the power is used to indicate the measured RSSI
   *   when receiving the packet.
   * * On transmission, the power is used to indicate what output
   *   power to use for transmission.
   *
   * @return the power.
   */
  virtual int8_t GetPower() const = 0;

  /**
   * Set the power parameter.
   * * On reception, the power is used to indicate the measured RSSI
   *   when receiving the packet.
   * * On transmission, the power is used to indicate what output
   *   power to use for transmission.
   *
   * @param[in] the power.
   */
  virtual void SetPower(int8_t power) = 0;
};

class PhyPacket;

/**
 * A platform-independent abstraction for an IEEE 802.15.4 transceiver.
 *
 * A platform-specific driver implements a Phy class that inherets the
 * PhyInterface.
 */
class PhyInterface {
 public:
  /**
   * \enum Error
   */
  enum Error {
    kErrorNone = 0,          ///< Success
    kErrorInvalidArgs = 1,   ///< One or more parameter values are invalid.
    kErrorInvalidState = 2,  ///< Transceiver is not in a proper state for the call.
    kErrorAbort = 3,         ///< Operation aborted.
  };

  /**
   * The abstraction exposes the following transceiver states:
   */
  enum State {
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
   * Callbacks from the PHY layer to the MAC layer.
   */
  class Callbacks {
   public:
    /**
     * Signals the transition from Receive to Idle.  Transition back to
     * Idle occurs whenever receiving a single PSDU or reception is
     * aborted for any reason.
     *
     * @param[in] psdu the PSDU that was received.  If reception was
     * aborted, psdu is NULL.
     * @param[in] psdu_length the length in bytes of the psdu.  Valid
     * values in the range [0, 127].
     * @param[in] channel the channel used in Receive.  Valid values in
     * the range [11, 26].
     * @param[in] rssi the measured RSSI for the received PSDU.
     *
     * @return The RSSI in dBm of the psdu.  If reception was aborted,
     * rssi is 127.
     */
    virtual void HandleReceiveDone(PhyPacket *packet, Error error) = 0;

    /**
     * Signals the transition from Transmit to Idle.  Transition back to
     * Idle occurs after transmission of the psdu is complete.
     *
     * @param[in] psdu the PSDU that was transmitted.
     * @param[in] psdu_length the length in bytes of the psdu.  Valid
     * values in the range [0, 127].
     * @param[in] channel the channel used when transmitting.  Valid
     * values in the range [11, 26].
     * @param[in] power the transmit power in dBm used when transmitting.
     */
    virtual void HandleTransmitDone(PhyPacket *packet, bool rx_pending, Error error) = 0;
  };

  /**
   * Constructor.
   *
   * @param[in] callbacks the object responsible for handling callbacks.
   */
  explicit PhyInterface(Callbacks *callbacks) { callbacks_ = callbacks; }

  /**
   * Intialize the transceiver.
   *
   * @return PhyInterface::kErrorNone if the transceiver initialized
   * successfully.
   */
  virtual Error Init() = 0;

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
  virtual Error Start() = 0;

  /**
   * Transitions the transceiver to Disabled from any state.  This
   * call is synchronous.
   *
   * @return PhyInterface::kErrorNone if the transceiver successfully
   * switches to Disabled.
   * @return PhyInterface::kErrorInvalidState if the transceiver could
   * not be switched to Disabled.
   */
  virtual Error Stop() = 0;

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
  virtual Error Sleep() = 0;

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
  virtual Error Idle() = 0;

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
  virtual Error Receive(PhyPacket *packet) = 0;

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
  virtual Error Transmit(PhyPacket *packet) = 0;

  /**
   * Gets the most recent noise floor measurement.  This call is
   * synchronous.
   *
   * @return The noise floor value in dBm when the noise floor value
   * is valid. 127 when noise floor value is invalid.
   */
  virtual int8_t GetNoiseFloor() = 0;

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
  virtual State GetState() = 0;

  virtual Error SetPanId(uint16_t panid) = 0;
  virtual Error SetExtendedAddress(uint8_t *address) = 0;
  virtual Error SetShortAddress(uint16_t address) = 0;

 protected:
  Callbacks *callbacks_;
};

}  // namespace Thread

#endif  // PLATFORM_COMMON_PHY_INTERFACE_H_
