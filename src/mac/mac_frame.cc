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

#include <common/code_utils.h>
#include <mac/mac_frame.h>
#include <platform/common/phy.h>
#include <stdio.h>

namespace Thread {

ThreadError MacFrame::InitMacHeader(uint16_t fcf, uint8_t sec_ctl) {
  uint8_t *bytes = GetPsdu();
  uint8_t length = 0;

  // Frame Control Field
  bytes[0] = fcf;
  bytes[1] = fcf >> 8;
  length += 2;

  // Sequence Number
  length++;

  // Destinatino PAN + Address
  switch (fcf & MacFrame::kFcfDstAddrMask) {
    case MacFrame::kFcfDstAddrNone:
      break;
    case MacFrame::kFcfDstAddrShort:
      length += 4;
      break;
    case MacFrame::kFcfDstAddrExt:
      length += 10;
      break;
    default:
      assert(false);
  }

  // Source PAN + Address
  switch (fcf & MacFrame::kFcfSrcAddrMask) {
    case MacFrame::kFcfSrcAddrNone:
      break;
    case MacFrame::kFcfSrcAddrShort:
      if ((fcf & MacFrame::kFcfPanidCompression) == 0)
        length += 2;
      length += 2;
      break;
    case MacFrame::kFcfSrcAddrExt:
      if ((fcf & MacFrame::kFcfPanidCompression) == 0)
        length += 2;
      length += 8;
      break;
    default:
      assert(false);
  }

  // Security Header
  if (fcf & MacFrame::kFcfSecurityEnabled) {
    bytes[length] = sec_ctl;
    if (sec_ctl & kSecLevelMask)
      length += 5;
    switch (sec_ctl & kKeyIdModeMask) {
      case kKeyIdMode0:
        break;
      case kKeyIdMode1:
        length += 1;
        break;
      case kKeyIdMode5:
        length += 5;
        break;
      case kKeyIdMode9:
        length += 9;
        break;
    }
  }

  // Command ID
  if ((fcf & kFcfFrameTypeMask) == kFcfFrameMacCmd)
    length++;

  SetPsduLength(length + GetFooterLength());

  return kThreadError_None;
}

uint8_t MacFrame::GetType() {
  return GetPsdu()[0] & MacFrame::kFcfFrameTypeMask;
}

bool MacFrame::GetSecurityEnabled() {
  return (GetPsdu()[0] & MacFrame::kFcfSecurityEnabled) != 0;
}

bool MacFrame::GetAckRequest() {
  return (GetPsdu()[0] & MacFrame::kFcfAckRequest) != 0;
}

ThreadError MacFrame::SetAckRequest(bool ack_request) {
  if (ack_request)
    GetPsdu()[0] |= MacFrame::kFcfAckRequest;
  else
    GetPsdu()[0] &= ~MacFrame::kFcfAckRequest;
  return kThreadError_None;
}

bool MacFrame::GetFramePending() {
  return (GetPsdu()[0] & MacFrame::kFcfFramePending) != 0;
}

ThreadError MacFrame::SetFramePending(bool frame_pending) {
  if (frame_pending)
    GetPsdu()[0] |= MacFrame::kFcfFramePending;
  else
    GetPsdu()[0] &= ~MacFrame::kFcfFramePending;
  return kThreadError_None;
}

uint8_t *MacFrame::FindSequence() {
  uint8_t *cur = GetPsdu();

  // Frame Control Field
  cur += 2;

  return cur;
}

ThreadError MacFrame::GetSequence(uint8_t *sequence) {
  uint8_t *buf = FindSequence();
  *sequence = buf[0];
  return kThreadError_None;
}

ThreadError MacFrame::SetSequence(uint8_t sequence) {
  uint8_t *buf = FindSequence();
  buf[0] = sequence;
  return kThreadError_None;
}

uint8_t *MacFrame::FindDstPanId() {
  uint8_t *cur = GetPsdu();

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
  VerifyOrExit((fcf & MacFrame::kFcfDstAddrMask) != MacFrame::kFcfDstAddrNone, cur = NULL);

  // Frame Control Field
  cur += 2;
  // Sequence Number
  cur += 1;

exit:
  return cur;
}

ThreadError MacFrame::GetDstPanId(MacPanId *panid) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  VerifyOrExit((buf = FindDstPanId()) != NULL, error = kThreadError_Parse);

  *panid = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];

exit:
  return error;
}

ThreadError MacFrame::SetDstPanId(MacPanId panid) {
  uint8_t *buf;

  VerifyOrExit((buf = FindDstPanId()) != NULL, assert(false));

  buf[0] = panid;
  buf[1] = panid >> 8;

exit:
  return kThreadError_None;
}

uint8_t *MacFrame::FindDstAddr() {
  uint8_t *cur = GetPsdu();

  // Frame Control Field
  cur += 2;
  // Sequence Number
  cur += 1;
  // Destination PAN
  cur += 2;

  return cur;
}

ThreadError MacFrame::GetDstAddr(MacAddress *address) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

  VerifyOrExit(buf = FindDstAddr(), error = kThreadError_Parse);

  switch (fcf & MacFrame::kFcfDstAddrMask) {
    case MacFrame::kFcfDstAddrShort:
      address->length = 2;
      address->address16 = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];
      break;
    case MacFrame::kFcfDstAddrExt:
      address->length = 8;
      for (int i = 0; i < 8; i++)
        address->address64.bytes[i] = buf[7-i];
      break;
    default:
      address->length = 0;
      break;
  }

exit:
  return error;
}

ThreadError MacFrame::SetDstAddr(MacAddr16 address16) {
  uint8_t *buf;

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
  assert((fcf & MacFrame::kFcfDstAddrMask) == MacFrame::kFcfDstAddrShort);
  VerifyOrExit((buf = FindDstAddr()) != NULL, assert(false));

  buf[0] = address16;
  buf[1] = address16 >> 8;

exit:
  return kThreadError_None;
}

ThreadError MacFrame::SetDstAddr(const MacAddr64 *address64) {
  uint8_t *buf;

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
  assert((fcf & MacFrame::kFcfDstAddrMask) == MacFrame::kFcfDstAddrExt);
  VerifyOrExit((buf = FindDstAddr()) != NULL, assert(false));

  for (int i = 0; i < 8; i++)
    buf[i] = address64->bytes[7-i];

exit:
  return kThreadError_None;
}

uint8_t *MacFrame::FindSrcPanId() {
  uint8_t *cur = GetPsdu();

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
  VerifyOrExit((fcf & MacFrame::kFcfDstAddrMask) != MacFrame::kFcfDstAddrNone ||
               (fcf & MacFrame::kFcfSrcAddrMask) != MacFrame::kFcfSrcAddrNone, cur = NULL);

  // Frame Control Field
  cur += 2;
  // Sequence Number
  cur += 1;

  if ((fcf & MacFrame::kFcfPanidCompression) != 0) {
    // Destination PAN + Address
    switch (fcf & MacFrame::kFcfDstAddrMask) {
      case MacFrame::kFcfDstAddrShort:
        cur += 4;
        break;
      case MacFrame::kFcfDstAddrExt:
        cur += 10;
        break;
    }
  }

exit:
  return cur;
}

ThreadError MacFrame::GetSrcPanId(MacPanId *panid) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  VerifyOrExit((buf = FindSrcPanId()) != NULL, error = kThreadError_Parse);

  *panid = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];

exit:
  return error;
}

ThreadError MacFrame::SetSrcPanId(MacPanId panid) {
  uint8_t *buf;

  VerifyOrExit((buf = FindSrcPanId()) != NULL, assert(false));

  buf[0] = panid;
  buf[1] = panid >> 8;

exit:
  return kThreadError_None;
}

uint8_t *MacFrame::FindSrcAddr() {
  uint8_t *cur = GetPsdu();

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

  // Frame Control Field
  cur += 2;
  // Sequence Number
  cur += 1;
  // Destination PAN + Address
  switch (fcf & MacFrame::kFcfDstAddrMask) {
    case MacFrame::kFcfDstAddrShort:
      cur += 4;
      break;
    case MacFrame::kFcfDstAddrExt:
      cur += 10;
      break;
  }
  // Source PAN
  if ((fcf & MacFrame::kFcfPanidCompression) == 0)
    cur += 2;

  return cur;
}

ThreadError MacFrame::GetSrcAddr(MacAddress *address) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

  VerifyOrExit((buf = FindSrcAddr()) != NULL, error = kThreadError_Parse);

  switch (fcf & MacFrame::kFcfSrcAddrMask) {
    case MacFrame::kFcfSrcAddrShort:
      address->length = 2;
      address->address16 = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];;
      break;
    case MacFrame::kFcfSrcAddrExt:
      address->length = 8;
      for (int i = 0; i < 8; i++)
        address->address64.bytes[i] = buf[7-i];
      break;
    default:
      address->length = 0;
      break;
  }

exit:
  return error;
}

ThreadError MacFrame::SetSrcAddr(MacAddr16 address16) {
  uint8_t *buf;

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
  assert((fcf & MacFrame::kFcfSrcAddrMask) == MacFrame::kFcfSrcAddrShort);
  VerifyOrExit((buf = FindSrcAddr()) != NULL, assert(false));

  buf[0] = address16;
  buf[1] = address16 >> 8;

exit:
  return kThreadError_None;
}

ThreadError MacFrame::SetSrcAddr(const MacAddr64 *address64) {
  uint8_t *buf;

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
  assert((fcf & MacFrame::kFcfSrcAddrMask) == MacFrame::kFcfSrcAddrExt);
  VerifyOrExit((buf = FindSrcAddr()) != NULL, assert(false));

  for (int i = 0; i < 8; i++)
    buf[i] = address64->bytes[7-i];

exit:
  return kThreadError_None;
}

uint8_t *MacFrame::FindSecurityHeader() {
  uint8_t *cur = GetPsdu();

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
  VerifyOrExit((fcf & MacFrame::kFcfSecurityEnabled) != 0, cur = NULL);

  // Frame Control Field
  cur += 2;
  // Sequence Number
  cur += 1;
  // Destination PAN + Address
  switch (fcf & MacFrame::kFcfDstAddrMask) {
    case MacFrame::kFcfDstAddrShort:
      cur += 4;
      break;
    case MacFrame::kFcfDstAddrExt:
      cur += 10;
      break;
  }
  // Source PAN + Address
  switch (fcf & MacFrame::kFcfSrcAddrMask) {
    case MacFrame::kFcfSrcAddrShort:
      if ((fcf & MacFrame::kFcfPanidCompression) == 0)
        cur += 2;
      cur += 2;
      break;
    case MacFrame::kFcfSrcAddrExt:
      if ((fcf & MacFrame::kFcfPanidCompression) == 0)
        cur += 2;
      cur += 8;
      break;
  }
  // Security Control
  assert((cur[0] & kKeyIdModeMask) == kKeyIdMode1);

exit:
  return cur;
}

ThreadError MacFrame::GetSecurityLevel(uint8_t *sec_level) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  VerifyOrExit((buf = FindSecurityHeader()) != NULL, error = kThreadError_Parse);

  *sec_level = buf[0] & kSecLevelMask;

exit:
  return error;
}

ThreadError MacFrame::GetFrameCounter(uint32_t *frame_counter) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  VerifyOrExit((buf = FindSecurityHeader()) != NULL, error = kThreadError_Parse);

  // Security Control
  buf++;

  *frame_counter = ((static_cast<uint32_t>(buf[3]) << 24) |
                    (static_cast<uint32_t>(buf[2]) << 16) |
                    (static_cast<uint32_t>(buf[1]) << 8) |
                    (static_cast<uint32_t>(buf[0])));

exit:
  return error;
}

ThreadError MacFrame::SetFrameCounter(uint32_t frame_counter) {
  uint8_t *buf;

  VerifyOrExit((buf = FindSecurityHeader()) != NULL, assert(false));

  // Security Control
  buf++;

  buf[0] = frame_counter;
  buf[1] = frame_counter >> 8;
  buf[2] = frame_counter >> 16;
  buf[3] = frame_counter >> 24;

exit:
  return kThreadError_None;
}

ThreadError MacFrame::GetKeyId(uint8_t *keyid) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  VerifyOrExit((buf = FindSecurityHeader()) != NULL, error = kThreadError_Parse);

  // Security Control + Frame Counter
  buf += 1 + 4;

  *keyid = buf[0];

exit:
  return error;
}

ThreadError MacFrame::SetKeyId(uint8_t keyid) {
  uint8_t *buf;

  VerifyOrExit((buf = FindSecurityHeader()) != NULL, assert(false));

  // Security Control + Frame Counter
  buf += 1 + 4;

  buf[0] = keyid;

exit:
  return kThreadError_None;
}

ThreadError MacFrame::GetCommandId(uint8_t *command_id) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  VerifyOrExit((buf = GetPayload()) != NULL, error = kThreadError_Parse);
  *command_id = buf[-1];

exit:
  return error;
}

ThreadError MacFrame::SetCommandId(uint8_t command_id) {
  ThreadError error = kThreadError_None;
  uint8_t *buf;

  VerifyOrExit((buf = GetPayload()) != NULL, error = kThreadError_Parse);
  buf[-1] = command_id;

exit:
  return error;
}

uint8_t MacFrame::GetLength() const {
  return GetPsduLength();
}

ThreadError MacFrame::SetLength(uint8_t length) {
  SetPsduLength(length);
  return kThreadError_None;
}

uint8_t MacFrame::GetHeaderLength() {
  return GetPayload() - GetPsdu();
}

uint8_t MacFrame::GetFooterLength() {
  uint8_t footer_length = 0;

  uint8_t *cur;
  VerifyOrExit((cur = FindSecurityHeader()) != NULL, ;);

  switch (cur[0] & kSecLevelMask) {
    case kSecNone:
    case kSecEnc:
      break;
    case kSecMic32:
    case kSecEncMic32:
      footer_length += 4;
      break;
    case kSecMic64:
    case kSecEncMic64:
      footer_length += 8;
      break;
    case kSecMic128:
    case kSecEncMic128:
      footer_length += 16;
      break;
  }

exit:
  // Frame Check Sequence
  footer_length += 2;

  return footer_length;
}

uint8_t MacFrame::GetMaxPayloadLength() {
  return kMTU - (GetHeaderLength() + GetFooterLength());
}

uint8_t MacFrame::GetPayloadLength() {
  return GetPsduLength() - (GetHeaderLength() + GetFooterLength());
}

ThreadError MacFrame::SetPayloadLength(uint8_t length) {
  SetPsduLength(GetHeaderLength() + GetFooterLength() + length);
  return kThreadError_None;
}

uint8_t *MacFrame::GetHeader() {
  return GetPsdu();
}

uint8_t *MacFrame::GetPayload() {
  uint8_t *cur = GetPsdu();

  uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

  // Frame Control
  cur += 2;
  // Sequence Number
  cur += 1;
  // Destination PAN + Address
  switch (fcf & MacFrame::kFcfDstAddrMask) {
    case MacFrame::kFcfDstAddrNone:
      break;
    case MacFrame::kFcfDstAddrShort:
      cur += 4;
      break;
    case MacFrame::kFcfDstAddrExt:
      cur += 10;
      break;
    default:
      ExitNow(cur = NULL);
  }
  // Source PAN + Address
  switch (fcf & MacFrame::kFcfSrcAddrMask) {
    case MacFrame::kFcfSrcAddrNone:
      break;
    case MacFrame::kFcfSrcAddrShort:
      if ((fcf & MacFrame::kFcfPanidCompression) == 0)
        cur += 2;
      cur += 2;
      break;
    case MacFrame::kFcfSrcAddrExt:
      if ((fcf & MacFrame::kFcfPanidCompression) == 0)
        cur += 2;
      cur += 8;
      break;
    default:
      ExitNow(cur = NULL);
  }
  // Security Control + Frame Counter + Key Identifier
  if ((fcf & MacFrame::kFcfSecurityEnabled) != 0) {
    VerifyOrExit((cur[0] & kKeyIdModeMask) == kKeyIdMode1, cur = NULL);
    cur += 1 + 4 + 1;
  }

  // Command ID
  if ((fcf & kFcfFrameTypeMask) == kFcfFrameMacCmd)
    cur++;

exit:
  return cur;
}

uint8_t *MacFrame::GetFooter() {
  return GetPsdu() + GetPsduLength() - GetFooterLength();
}

}  // namespace Thread

