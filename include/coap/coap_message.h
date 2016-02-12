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

#ifndef COAP_COAP_MESSAGE_H_
#define COAP_COAP_MESSAGE_H_

#include <common/message.h>

namespace Thread {

class CoapMessage {
 public:
  CoapMessage();
  ThreadError Init();
  ThreadError FromMessage(Thread::Message *message);

  enum {
    kVersionMask = 0xc0,
    kVersionOffset = 6,
  };
  uint8_t GetVersion() const;
  ThreadError SetVersion(uint8_t version);

  enum {
    kTypeMask = 0x30,
  };

  enum Type {
    kTypeConfirmable = 0x00,
    kTypeNonConfirmable = 0x10,
    kTypeAcknowledgment = 0x20,
    kTypeReset = 0x30,
  };

  Type GetType() const;
  ThreadError SetType(Type type);

  enum Code {
    kCodeGet = 0x01,
    kCodePost = 0x02,
    kCodePut = 0x03,
    kCodeDelete = 0x04,
    kCodeChanged = 0x44,
    kCodeContent = 0x45,
  };

  Code GetCode() const;
  ThreadError SetCode(Code code);

  uint16_t GetMessageId() const;
  ThreadError SetMessageId(uint16_t message_id);

  enum {
    kTokenLengthMask = 0x0f,
    kTokenLengthOffset = 0,
    kTokenOffset = 4,
    kMaxTokenLength = 8,
  };
  uint8_t GetTokenLength() const;
  const uint8_t *GetToken(uint8_t *token_length) const;
  ThreadError SetToken(uint8_t *token, uint8_t token_length);

  struct Option {
    enum {
      kOptionDeltaOffset = 4,
      kOptionUriPath = 11,
      kOptionContentFormat = 12,
    };
    uint16_t number;
    uint16_t length;
    const uint8_t *value;
  };
  ThreadError AppendOption(const Option *option);
  ThreadError AppendUriPathOptions(const char *uri_path);

  enum {
    kApplicationOctetStream = 42,
  };
  ThreadError AppendContentFormatOption(uint8_t type);
  const Option *GetCurrentOption() const;
  const Option *GetNextOption();

  ThreadError Finalize();

  const uint8_t *GetHeaderBytes() const;
  uint8_t GetHeaderLength() const;

 private:
  enum {
    kMaxHeaderLength = 128,
  };
  uint8_t header_[kMaxHeaderLength];
  uint8_t header_length_;
  uint16_t option_last_;
  uint16_t next_option_offset_;
  Option option_;
};

}  // namespace Thread

#endif  // COAP_COAP_MESSAGE_H_
