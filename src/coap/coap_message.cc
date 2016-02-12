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

#include <coap/coap_message.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/thread_error.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

CoapMessage::CoapMessage() {
  header_length_ = 4;
  option_last_ = 0;
  next_option_offset_ = 0;
  option_ = {0, 0, NULL};
  memset(header_, 0, sizeof(header_));
}

ThreadError CoapMessage::Init() {
  header_length_ = 4;
  option_last_ = 0;
  return kThreadError_None;
}

ThreadError CoapMessage::FromMessage(Thread::Message *message) {
  ThreadError error = kThreadError_Parse;
  uint16_t offset = message->GetOffset();
  uint16_t length = message->GetLength() - message->GetOffset();

  VerifyOrExit(length >= kTokenOffset, error = kThreadError_Parse);
  message->Read(offset, kTokenOffset, header_);
  header_length_ = kTokenOffset;
  offset += kTokenOffset;
  length -= kTokenOffset;

  VerifyOrExit(GetVersion() == 1, error = kThreadError_Parse);

  uint8_t token_length;
  token_length = GetTokenLength();
  VerifyOrExit(token_length < 8 && token_length < length, error = kThreadError_Parse);
  message->Read(offset, token_length, header_ + header_length_);
  header_length_ += token_length;
  offset += token_length;
  length -= token_length;

  bool first_option;
  first_option = true;
  while (length > 0) {
    message->Read(offset, 5, header_ + header_length_);

    if (header_[header_length_] == 0xff) {
      header_length_++;
      ExitNow(error = kThreadError_None);
    }

    uint8_t option_delta;
    uint16_t option_length;

    option_delta = header_[header_length_] >> 4;
    option_length = header_[header_length_] & 0xf;
    header_length_++;
    offset++;
    length--;

    if (option_delta < 13) {
      // do nothing
    } else if (option_delta == 13) {
      option_delta = 13 + header_[header_length_];
      header_length_++;
      offset++;
      length--;
    } else if (option_delta == 14) {
      option_delta = 269 + ((static_cast<uint16_t>(header_[header_length_]) << 8) | header_[header_length_ + 1]);
      header_length_ += 2;
      offset += 2;
      length -= 2;
    } else {
      ExitNow(error = kThreadError_Parse);
    }

    if (option_length < 13) {
      // do nothing
    } else if (option_length == 13) {
      option_length = 13 + header_[header_length_];
      header_length_++;
      offset++;
      length--;
    } else if (option_length == 14) {
      option_length = 269 + ((static_cast<uint16_t>(header_[header_length_]) << 8) | header_[header_length_ + 1]);
      header_length_ += 2;
      offset += 2;
      length -= 2;
    } else {
      ExitNow(error = kThreadError_Parse);
    }

    if (first_option) {
      option_.number = option_delta;
      option_.length = option_length;
      option_.value = header_ + header_length_;
      next_option_offset_ = header_length_ + option_length;
      first_option = false;
    }

    VerifyOrExit(option_length <= length, error = kThreadError_Parse);
    message->Read(offset, option_length, header_ + header_length_);
    header_length_ += option_length;
    offset += option_length;
    length -= option_length;
  }

exit:
  return error;
}

uint8_t CoapMessage::GetVersion() const {
  return (header_[0] & kVersionMask) >> kVersionOffset;
}

ThreadError CoapMessage::SetVersion(uint8_t version) {
  header_[0] &= ~kVersionMask;
  header_[0] |= version << kVersionOffset;
  return kThreadError_None;
}

CoapMessage::Type CoapMessage::GetType() const {
  return static_cast<CoapMessage::Type>(header_[0] & kTypeMask);
}

ThreadError CoapMessage::SetType(CoapMessage::Type type) {
  header_[0] &= ~kTypeMask;
  header_[0] |= type;
  return kThreadError_None;
}

CoapMessage::Code CoapMessage::GetCode() const {
  return static_cast<CoapMessage::Code>(header_[1]);
}

ThreadError CoapMessage::SetCode(CoapMessage::Code code) {
  header_[1] = code;
  return kThreadError_None;
}

uint16_t CoapMessage::GetMessageId() const {
  return (static_cast<uint16_t>(header_[2]) << 8) | header_[3];
}

ThreadError CoapMessage::SetMessageId(uint16_t message_id) {
  header_[2] = message_id >> 8;
  header_[3] = message_id;
  return kThreadError_None;
}

const uint8_t *CoapMessage::GetToken(uint8_t *token_length) const {
  if (token_length)
    *token_length = GetTokenLength();
  return header_ + kTokenOffset;
}

uint8_t CoapMessage::GetTokenLength() const {
  return (header_[0] & kTokenLengthMask) >> kTokenLengthOffset;
}

ThreadError CoapMessage::SetToken(uint8_t *token, uint8_t token_length) {
  header_[0] &= ~kTokenLengthMask;
  header_[0] |= token_length << kTokenLengthOffset;
  memcpy(header_ + kTokenOffset, token, token_length);
  header_length_ += token_length;
  return kThreadError_None;
}

ThreadError CoapMessage::AppendOption(const Option *option) {
  uint8_t *buf = header_ + header_length_;
  uint8_t *cur = buf + 1;

  uint16_t delta = option->number - option_last_;
  if (delta < 13) {
    *buf = delta << Option::kOptionDeltaOffset;
  } else if (delta < 269) {
    *buf |= 13 << Option::kOptionDeltaOffset;
    *cur++ = delta - 13;
  } else {
    *buf |= 14 << Option::kOptionDeltaOffset;
    delta -= 269;
    *cur++ = delta >> 8;
    *cur++ = delta;
  }

  if (option->length < 13) {
    *buf |= option->length;
  } else if (option->length < 269) {
    *buf |= 0x0d;
    *cur++ = option->length - 13;
  } else {
    *buf |= 0x0e;
    uint16_t tmp_length = option->length - 269;
    *cur++ = tmp_length >> 8;
    *cur++ = tmp_length;
  }

  memcpy(cur, option->value, option->length);
  cur += option->length;

  header_length_ += cur - buf;
  option_last_ = option->number;

  return kThreadError_None;
}

ThreadError CoapMessage::AppendUriPathOptions(const char *uri_path) {
  const char *cur = uri_path;
  const char *end;

  CoapMessage::Option coap_option;
  coap_option.number = Option::kOptionUriPath;

  while ((end = strchr(cur, '/')) != NULL) {
    coap_option.length = end - cur;
    coap_option.value = reinterpret_cast<const uint8_t*>(cur);
    AppendOption(&coap_option);
    cur = end + 1;
  }

  coap_option.length = strlen(cur);
  coap_option.value = reinterpret_cast<const uint8_t*>(cur);
  AppendOption(&coap_option);

  return kThreadError_None;
}

ThreadError CoapMessage::AppendContentFormatOption(uint8_t type) {
  Option coap_option;

  coap_option.number = Option::kOptionContentFormat;
  coap_option.length = 1;
  coap_option.value = &type;
  AppendOption(&coap_option);

  return kThreadError_None;
}

const CoapMessage::Option *CoapMessage::GetCurrentOption() const {
  return &option_;
}

const CoapMessage::Option *CoapMessage::GetNextOption() {
  Option *rval = NULL;

  VerifyOrExit(next_option_offset_ < header_length_, ;);

  uint8_t option_delta;
  uint16_t option_length;

  option_delta = header_[next_option_offset_] >> 4;
  option_length = header_[next_option_offset_] & 0xf;
  next_option_offset_++;

  if (option_delta < 13) {
    // do nothing
  } else if (option_delta == 13) {
    option_delta = 13 + header_[next_option_offset_];
    next_option_offset_++;
  } else if (option_delta == 14) {
    option_delta = 269 + ((static_cast<uint16_t>(header_[next_option_offset_]) << 8) |
                          header_[next_option_offset_ + 1]);
    next_option_offset_ += 2;
  } else {
    ExitNow();
  }

  if (option_length < 13) {
    // do nothing
  } else if (option_length == 13) {
    option_length = 13 + header_[next_option_offset_];
    next_option_offset_++;
  } else if (option_length == 14) {
    option_length = 269 + ((static_cast<uint16_t>(header_[next_option_offset_]) << 8) |
                           header_[next_option_offset_ + 1]);
    next_option_offset_ += 2;
  } else {
    ExitNow();
  }

  option_.number += option_delta;
  option_.length = option_length;
  option_.value = header_ + next_option_offset_;
  next_option_offset_ += option_length;
  rval = &option_;

exit:
  return rval;
}

ThreadError CoapMessage::Finalize() {
  header_[header_length_++] = 0xff;
  return kThreadError_None;
}

const uint8_t *CoapMessage::GetHeaderBytes() const {
  return header_;
}

uint8_t CoapMessage::GetHeaderLength() const {
  return header_length_;
}

}  // namespace Thread
