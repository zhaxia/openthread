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

#ifndef PLATFORM_COMMON_UART_INTERFACE_H_
#define PLATFORM_COMMON_UART_INTERFACE_H_

#include <common/thread_error.h>
#include <stdint.h>

namespace Thread {

/**
 * The interface to the UART object that supports the Thread stack's
 * serial communication.
 */
class UartInterface {
 public:
  /**
   * Callbacks from the UART object.
   */
  class Callbacks {
   public:
    /**
     * Signals the reception of data.
     *
     * @param[in] buf Pointer to the data buffer containing the
     * received bytes.
     * @param[in] buf_length Length of data buffer.
     */
    virtual void HandleReceive(uint8_t *buf, uint16_t buf_length) = 0;

    /**
     * Signals the completion of sending data.
     */
    virtual void HandleSendDone() = 0;
  };

  /**
   * Constructor.
   *
   * @param[in] callbacks the object responsible for handling callbacks.
   */
  explicit UartInterface(Callbacks *callbacks) { callbacks_ = callbacks; }

  /**
   * Start the UART peripheral.
   *
   * @retval Thread::kThreadError_None Start was successful.
   * @retval Thread::kThreadError_Error Start was not successful.
   */
  virtual ThreadError Start() = 0;

  /**
   * Stop the UART peripheral.
   *
   * @retval Thread::kThreadError_None Stop was successful.
   * @retval Thread::kThreadError_Error Stop was not successful.
   */
  virtual ThreadError Stop() = 0;

  /**
   * Send bytes over the UART.
   *
   * @param[in] buf Pointer to the data buffer.
   * @param[in] buf_length Length of the data buffer.
   *
   * @retval Thread::kThreadError_None Send was successful.
   * @retval Thread::kThreadError_Error Send was not successful.
   */
  virtual ThreadError Send(const uint8_t *buf, uint16_t buf_length) = 0;

  Callbacks *callbacks_;
};

}  // namespace Thread

#endif  // PLATFORM_COMMON_UART_INTERFACE_H_
