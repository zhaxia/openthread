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

#ifndef NCP_HDLC_H_
#define NCP_HDLC_H_

#include <common/message.h>
#include <common/thread_error.h>
#include <platform/common/uart_interface.h>

namespace Thread {

class Hdlc: public UartInterface::Callbacks {
 public:
  class Callbacks {
   public:
    virtual void HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length) = 0;
    virtual void HandleSendDone() = 0;
    virtual void HandleSendMessageDone() = 0;
  };

  explicit Hdlc(UartInterface *uart, Callbacks *callbacks);
  ThreadError Start();
  ThreadError Stop();

  ThreadError Send(uint8_t protocol, uint8_t *frame, uint16_t frame_length);
  ThreadError SendMessage(uint8_t protocol, Message *message);

  void HandleReceive(uint8_t *buf, uint16_t buf_length) final;
  void HandleSendDone() final;

 private:
  enum {
    kFlagSequence = 0x7e,
    kEscapeSequence = 0x7d,
  };

  enum State {
    kStateNoSync = 0,
    kStateSync,
    kStateEscaped,
  };

  uint16_t AppendSendByte(uint8_t byte, uint16_t fcs);
  Callbacks *callbacks_;

  State receive_state_ = kStateNoSync;
  uint8_t receive_frame_[1024];
  uint16_t receive_frame_length_ = 0;
  uint16_t receive_fcs_ = 0;

  uint8_t send_frame_[1024];
  uint16_t send_frame_length_ = 0;
  uint8_t send_protocol_ = 0;
  Message *send_message_ = NULL;

  UartInterface *uart_;
};

}  // namespace Thread

#endif  // NCP_HDLC_H_
