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

namespace Thread {
namespace Mac {

ThreadError Frame::InitMacHeader(uint16_t fcf, uint8_t sec_ctl)
{
    uint8_t *bytes = GetPsdu();
    uint8_t length = 0;

    // Frame Control Field
    bytes[0] = fcf;
    bytes[1] = fcf >> 8;
    length += 2;

    // Sequence Number
    length++;

    // Destinatino PAN + Address
    switch (fcf & Frame::kFcfDstAddrMask)
    {
    case Frame::kFcfDstAddrNone:
        break;

    case Frame::kFcfDstAddrShort:
        length += 4;
        break;

    case Frame::kFcfDstAddrExt:
        length += 10;
        break;

    default:
        assert(false);
    }

    // Source PAN + Address
    switch (fcf & Frame::kFcfSrcAddrMask)
    {
    case Frame::kFcfSrcAddrNone:
        break;

    case Frame::kFcfSrcAddrShort:
        if ((fcf & Frame::kFcfPanidCompression) == 0)
        {
            length += 2;
        }

        length += 2;
        break;

    case Frame::kFcfSrcAddrExt:
        if ((fcf & Frame::kFcfPanidCompression) == 0)
        {
            length += 2;
        }

        length += 8;
        break;

    default:
        assert(false);
    }

    // Security Header
    if (fcf & Frame::kFcfSecurityEnabled)
    {
        bytes[length] = sec_ctl;

        if (sec_ctl & kSecLevelMask)
        {
            length += 5;
        }

        switch (sec_ctl & kKeyIdModeMask)
        {
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
    {
        length++;
    }

    SetPsduLength(length + GetFooterLength());

    return kThreadError_None;
}

uint8_t Frame::GetType()
{
    return GetPsdu()[0] & Frame::kFcfFrameTypeMask;
}

bool Frame::GetSecurityEnabled()
{
    return (GetPsdu()[0] & Frame::kFcfSecurityEnabled) != 0;
}

bool Frame::GetAckRequest()
{
    return (GetPsdu()[0] & Frame::kFcfAckRequest) != 0;
}

ThreadError Frame::SetAckRequest(bool ack_request)
{
    if (ack_request)
    {
        GetPsdu()[0] |= Frame::kFcfAckRequest;
    }
    else
    {
        GetPsdu()[0] &= ~Frame::kFcfAckRequest;
    }

    return kThreadError_None;
}

bool Frame::GetFramePending()
{
    return (GetPsdu()[0] & Frame::kFcfFramePending) != 0;
}

ThreadError Frame::SetFramePending(bool frame_pending)
{
    if (frame_pending)
    {
        GetPsdu()[0] |= Frame::kFcfFramePending;
    }
    else
    {
        GetPsdu()[0] &= ~Frame::kFcfFramePending;
    }

    return kThreadError_None;
}

uint8_t *Frame::FindSequence()
{
    uint8_t *cur = GetPsdu();

    // Frame Control Field
    cur += 2;

    return cur;
}

ThreadError Frame::GetSequence(uint8_t &sequence)
{
    uint8_t *buf = FindSequence();
    sequence = buf[0];
    return kThreadError_None;
}

ThreadError Frame::SetSequence(uint8_t sequence)
{
    uint8_t *buf = FindSequence();
    buf[0] = sequence;
    return kThreadError_None;
}

uint8_t *Frame::FindDstPanId()
{
    uint8_t *cur = GetPsdu();
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    VerifyOrExit((fcf & Frame::kFcfDstAddrMask) != Frame::kFcfDstAddrNone, cur = NULL);

    // Frame Control Field
    cur += 2;
    // Sequence Number
    cur += 1;

exit:
    return cur;
}

ThreadError Frame::GetDstPanId(PanId &panid)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;

    VerifyOrExit((buf = FindDstPanId()) != NULL, error = kThreadError_Parse);

    panid = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];

exit:
    return error;
}

ThreadError Frame::SetDstPanId(PanId panid)
{
    uint8_t *buf;

    buf = FindDstPanId();
    assert(buf != NULL);

    buf[0] = panid;
    buf[1] = panid >> 8;

    return kThreadError_None;
}

uint8_t *Frame::FindDstAddr()
{
    uint8_t *cur = GetPsdu();

    // Frame Control Field
    cur += 2;
    // Sequence Number
    cur += 1;
    // Destination PAN
    cur += 2;

    return cur;
}

ThreadError Frame::GetDstAddr(Address &address)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    VerifyOrExit(buf = FindDstAddr(), error = kThreadError_Parse);

    switch (fcf & Frame::kFcfDstAddrMask)
    {
    case Frame::kFcfDstAddrShort:
        address.length = 2;
        address.address16 = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];
        break;

    case Frame::kFcfDstAddrExt:
        address.length = 8;

        for (int i = 0; i < 8; i++)
        {
            address.address64.bytes[i] = buf[7 - i];
        }

        break;

    default:
        address.length = 0;
        break;
    }

exit:
    return error;
}

ThreadError Frame::SetDstAddr(Address16 address16)
{
    uint8_t *buf;
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    assert((fcf & Frame::kFcfDstAddrMask) == Frame::kFcfDstAddrShort);

    buf = FindDstAddr();
    assert(buf != NULL);

    buf[0] = address16;
    buf[1] = address16 >> 8;

    return kThreadError_None;
}

ThreadError Frame::SetDstAddr(const Address64 &address64)
{
    uint8_t *buf;
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    assert((fcf & Frame::kFcfDstAddrMask) == Frame::kFcfDstAddrExt);

    buf = FindDstAddr();
    assert(buf != NULL);

    for (int i = 0; i < 8; i++)
    {
        buf[i] = address64.bytes[7 - i];
    }

    return kThreadError_None;
}

uint8_t *Frame::FindSrcPanId()
{
    uint8_t *cur = GetPsdu();
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    VerifyOrExit((fcf & Frame::kFcfDstAddrMask) != Frame::kFcfDstAddrNone ||
                 (fcf & Frame::kFcfSrcAddrMask) != Frame::kFcfSrcAddrNone, cur = NULL);

    // Frame Control Field
    cur += 2;
    // Sequence Number
    cur += 1;

    if ((fcf & Frame::kFcfPanidCompression) == 0)
    {
        // Destination PAN + Address
        switch (fcf & Frame::kFcfDstAddrMask)
        {
        case Frame::kFcfDstAddrShort:
            cur += 4;
            break;

        case Frame::kFcfDstAddrExt:
            cur += 10;
            break;
        }
    }

exit:
    return cur;
}

ThreadError Frame::GetSrcPanId(PanId &panid)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;

    VerifyOrExit((buf = FindSrcPanId()) != NULL, error = kThreadError_Parse);

    panid = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];

exit:
    return error;
}

ThreadError Frame::SetSrcPanId(PanId panid)
{
    uint8_t *buf;

    buf = FindSrcPanId();
    assert(buf != NULL);

    buf[0] = panid;
    buf[1] = panid >> 8;

    return kThreadError_None;
}

uint8_t *Frame::FindSrcAddr()
{
    uint8_t *cur = GetPsdu();
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    // Frame Control Field
    cur += 2;
    // Sequence Number
    cur += 1;

    // Destination PAN + Address
    switch (fcf & Frame::kFcfDstAddrMask)
    {
    case Frame::kFcfDstAddrShort:
        cur += 4;
        break;

    case Frame::kFcfDstAddrExt:
        cur += 10;
        break;
    }

    // Source PAN
    if ((fcf & Frame::kFcfPanidCompression) == 0)
    {
        cur += 2;
    }

    return cur;
}

ThreadError Frame::GetSrcAddr(Address &address)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    VerifyOrExit((buf = FindSrcAddr()) != NULL, error = kThreadError_Parse);

    switch (fcf & Frame::kFcfSrcAddrMask)
    {
    case Frame::kFcfSrcAddrShort:
        address.length = 2;
        address.address16 = (static_cast<uint16_t>(buf[1]) << 8) | buf[0];;
        break;

    case Frame::kFcfSrcAddrExt:
        address.length = 8;

        for (int i = 0; i < 8; i++)
        {
            address.address64.bytes[i] = buf[7 - i];
        }

        break;

    default:
        address.length = 0;
        break;
    }

exit:
    return error;
}

ThreadError Frame::SetSrcAddr(Address16 address16)
{
    uint8_t *buf;
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    assert((fcf & Frame::kFcfSrcAddrMask) == Frame::kFcfSrcAddrShort);

    buf = FindSrcAddr();
    assert(buf != NULL);

    buf[0] = address16;
    buf[1] = address16 >> 8;

    return kThreadError_None;
}

ThreadError Frame::SetSrcAddr(const Address64 &address64)
{
    uint8_t *buf;
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    assert((fcf & Frame::kFcfSrcAddrMask) == Frame::kFcfSrcAddrExt);

    buf = FindSrcAddr();
    assert(buf != NULL);

    for (int i = 0; i < 8; i++)
    {
        buf[i] = address64.bytes[7 - i];
    }

    return kThreadError_None;
}

uint8_t *Frame::FindSecurityHeader()
{
    uint8_t *cur = GetPsdu();
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];

    VerifyOrExit((fcf & Frame::kFcfSecurityEnabled) != 0, cur = NULL);

    // Frame Control Field
    cur += 2;
    // Sequence Number
    cur += 1;

    // Destination PAN + Address
    switch (fcf & Frame::kFcfDstAddrMask)
    {
    case Frame::kFcfDstAddrShort:
        cur += 4;
        break;

    case Frame::kFcfDstAddrExt:
        cur += 10;
        break;
    }

    // Source PAN + Address
    switch (fcf & Frame::kFcfSrcAddrMask)
    {
    case Frame::kFcfSrcAddrShort:
        if ((fcf & Frame::kFcfPanidCompression) == 0)
        {
            cur += 2;
        }

        cur += 2;
        break;

    case Frame::kFcfSrcAddrExt:
        if ((fcf & Frame::kFcfPanidCompression) == 0)
        {
            cur += 2;
        }

        cur += 8;
        break;
    }

exit:
    return cur;
}

ThreadError Frame::GetSecurityLevel(uint8_t &sec_level)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;

    VerifyOrExit((buf = FindSecurityHeader()) != NULL, error = kThreadError_Parse);

    sec_level = buf[0] & kSecLevelMask;

exit:
    return error;
}

ThreadError Frame::GetFrameCounter(uint32_t &frame_counter)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;

    VerifyOrExit((buf = FindSecurityHeader()) != NULL, error = kThreadError_Parse);

    // Security Control
    buf++;

    frame_counter = ((static_cast<uint32_t>(buf[3]) << 24) |
                     (static_cast<uint32_t>(buf[2]) << 16) |
                     (static_cast<uint32_t>(buf[1]) << 8) |
                     (static_cast<uint32_t>(buf[0])));

exit:
    return error;
}

ThreadError Frame::SetFrameCounter(uint32_t frame_counter)
{
    uint8_t *buf;

    buf = FindSecurityHeader();
    assert(buf != NULL);

    // Security Control
    buf++;

    buf[0] = frame_counter;
    buf[1] = frame_counter >> 8;
    buf[2] = frame_counter >> 16;
    buf[3] = frame_counter >> 24;

    return kThreadError_None;
}

ThreadError Frame::GetKeyId(uint8_t &keyid)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;

    VerifyOrExit((buf = FindSecurityHeader()) != NULL, error = kThreadError_Parse);

    // Security Control + Frame Counter
    buf += 1 + 4;

    keyid = buf[0];

exit:
    return error;
}

ThreadError Frame::SetKeyId(uint8_t keyid)
{
    uint8_t *buf;

    buf = FindSecurityHeader();
    assert(buf != NULL);

    // Security Control + Frame Counter
    buf += 1 + 4;

    buf[0] = keyid;

    return kThreadError_None;
}

ThreadError Frame::GetCommandId(uint8_t &command_id)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;

    VerifyOrExit((buf = GetPayload()) != NULL, error = kThreadError_Parse);
    command_id = buf[-1];

exit:
    return error;
}

ThreadError Frame::SetCommandId(uint8_t command_id)
{
    ThreadError error = kThreadError_None;
    uint8_t *buf;

    VerifyOrExit((buf = GetPayload()) != NULL, error = kThreadError_Parse);
    buf[-1] = command_id;

exit:
    return error;
}

uint8_t Frame::GetLength() const
{
    return GetPsduLength();
}

ThreadError Frame::SetLength(uint8_t length)
{
    SetPsduLength(length);
    return kThreadError_None;
}

uint8_t Frame::GetHeaderLength()
{
    return GetPayload() - GetPsdu();
}

uint8_t Frame::GetFooterLength()
{
    uint8_t footer_length = 0;
    uint8_t *cur;

    VerifyOrExit((cur = FindSecurityHeader()) != NULL, ;);

    switch (cur[0] & kSecLevelMask)
    {
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

uint8_t Frame::GetMaxPayloadLength()
{
    return kMTU - (GetHeaderLength() + GetFooterLength());
}

uint8_t Frame::GetPayloadLength()
{
    return GetPsduLength() - (GetHeaderLength() + GetFooterLength());
}

ThreadError Frame::SetPayloadLength(uint8_t length)
{
    SetPsduLength(GetHeaderLength() + GetFooterLength() + length);
    return kThreadError_None;
}

uint8_t *Frame::GetHeader()
{
    return GetPsdu();
}

uint8_t *Frame::GetPayload()
{
    uint8_t *cur = GetPsdu();
    uint16_t fcf = (static_cast<uint16_t>(GetPsdu()[1]) << 8) | GetPsdu()[0];
    uint8_t sec_ctl;

    // Frame Control
    cur += 2;
    // Sequence Number
    cur += 1;

    // Destination PAN + Address
    switch (fcf & Frame::kFcfDstAddrMask)
    {
    case Frame::kFcfDstAddrNone:
        break;

    case Frame::kFcfDstAddrShort:
        cur += 4;
        break;

    case Frame::kFcfDstAddrExt:
        cur += 10;
        break;

    default:
        ExitNow(cur = NULL);
    }

    // Source PAN + Address
    switch (fcf & Frame::kFcfSrcAddrMask)
    {
    case Frame::kFcfSrcAddrNone:
        break;

    case Frame::kFcfSrcAddrShort:
        if ((fcf & Frame::kFcfPanidCompression) == 0)
        {
            cur += 2;
        }

        cur += 2;
        break;

    case Frame::kFcfSrcAddrExt:
        if ((fcf & Frame::kFcfPanidCompression) == 0)
        {
            cur += 2;
        }

        cur += 8;
        break;

    default:
        ExitNow(cur = NULL);
    }

    // Security Control + Frame Counter + Key Identifier
    if ((fcf & Frame::kFcfSecurityEnabled) != 0)
    {
        sec_ctl = *cur;

        if (sec_ctl & kSecLevelMask)
        {
            cur += 5;
        }

        switch (sec_ctl & kKeyIdModeMask)
        {
        case kKeyIdMode0:
            break;

        case kKeyIdMode1:
            cur += 1;
            break;

        case kKeyIdMode5:
            cur += 5;
            break;

        case kKeyIdMode9:
            cur += 9;
            break;
        }
    }

    // Command ID
    if ((fcf & kFcfFrameTypeMask) == kFcfFrameMacCmd)
    {
        cur++;
    }

exit:
    return cur;
}

uint8_t *Frame::GetFooter()
{
    return GetPsdu() + GetPsduLength() - GetFooterLength();
}

}  // namespace Mac
}  // namespace Thread

