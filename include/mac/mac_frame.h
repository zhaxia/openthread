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

#ifndef MAC_FRAME_H_
#define MAC_FRAME_H_

#include <stdint.h>
#include <common/thread_error.h>
#include <platform/common/phy.h>

namespace Thread {
namespace Mac {

enum
{
    kShortAddrBroadcast = 0xffff,
    kShortAddrInvalid = 0xfffe,
};

typedef uint16_t PanId;
typedef uint16_t Address16;

struct Address64
{
    uint8_t bytes[8];
};

struct Address
{
    uint8_t length;
    union
    {
        Address16 address16;
        Address64 address64;
    };
};

class Frame: public PhyPacket
{
public:
    enum
    {
        kMTU = 127,

        kFcfFrameBeacon       = 0 << 0,
        kFcfFrameData         = 1 << 0,
        kFcfFrameAck          = 2 << 0,
        kFcfFrameMacCmd       = 3 << 0,
        kFcfFrameTypeMask     = 7 << 0,
        kFcfSecurityEnabled   = 1 << 3,
        kFcfFramePending      = 1 << 4,
        kFcfAckRequest        = 1 << 5,
        kFcfPanidCompression  = 1 << 6,
        kFcfDstAddrNone       = 0 << 10,
        kFcfDstAddrShort      = 2 << 10,
        kFcfDstAddrExt        = 3 << 10,
        kFcfDstAddrMask       = 3 << 10,
        kFcfFrameVersion2006  = 1 << 12,
        kFcfFrameVersionMask  = 3 << 13,
        kFcfSrcAddrNone       = 0 << 14,
        kFcfSrcAddrShort      = 2 << 14,
        kFcfSrcAddrExt        = 3 << 14,
        kFcfSrcAddrMask       = 3 << 14,

        kSecNone              = 0 << 0,
        kSecMic32             = 1 << 0,
        kSecMic64             = 2 << 0,
        kSecMic128            = 3 << 0,
        kSecEnc               = 4 << 0,
        kSecEncMic32          = 5 << 0,
        kSecEncMic64          = 6 << 0,
        kSecEncMic128         = 7 << 0,
        kSecLevelMask         = 7 << 0,

        kKeyIdMode0           = 0 << 3,
        kKeyIdMode1           = 1 << 3,
        kKeyIdMode5           = 2 << 3,
        kKeyIdMode9           = 3 << 3,
        kKeyIdModeMask        = 3 << 3,

        kMacCmdAssociationRequest          = 1,
        kMacCmdAssociationResponse         = 2,
        kMacCmdDisassociationNotification  = 3,
        kMacCmdDataRequest                 = 4,
        kMacCmdPanidConflictNotification   = 5,
        kMacCmdOrphanNotification          = 6,
        kMacCmdBeaconRequest               = 7,
        kMacCmdCoordinatorRealignment      = 8,
        kMacCmdGtsRequest                  = 9,
    };

    ThreadError InitMacHeader(uint16_t fcf, uint8_t sec_ctl);
    uint8_t GetType();

    bool GetSecurityEnabled();

    bool GetFramePending();
    ThreadError SetFramePending(bool frame_pending);

    bool GetAckRequest();
    ThreadError SetAckRequest(bool ack_request);

    ThreadError GetSequence(uint8_t &sequence);
    ThreadError SetSequence(uint8_t sequence);

    ThreadError GetDstPanId(PanId &panid);
    ThreadError SetDstPanId(PanId panid);

    ThreadError GetDstAddr(Address &address);
    ThreadError SetDstAddr(Address16 address16);
    ThreadError SetDstAddr(const Address64 &address64);

    ThreadError GetSrcPanId(PanId &panid);
    ThreadError SetSrcPanId(PanId panid);

    ThreadError GetSrcAddr(Address &address);
    ThreadError SetSrcAddr(Address16 address16);
    ThreadError SetSrcAddr(const Address64 &address64);

    ThreadError GetSecurityLevel(uint8_t &sec_level);

    ThreadError GetFrameCounter(uint32_t &frame_counter);
    ThreadError SetFrameCounter(uint32_t frame_counter);

    ThreadError GetKeyId(uint8_t &id);
    ThreadError SetKeyId(uint8_t id);

    ThreadError GetCommandId(uint8_t &command_id);
    ThreadError SetCommandId(uint8_t command_id);

    uint8_t GetLength() const;
    ThreadError SetLength(uint8_t length);

    uint8_t GetHeaderLength();
    uint8_t GetFooterLength();

    uint8_t GetPayloadLength();
    uint8_t GetMaxPayloadLength();
    ThreadError SetPayloadLength(uint8_t length);

    uint8_t GetChannel() const { return m_channel; }
    void SetChannel(uint8_t channel) { m_channel = channel; }

    int8_t GetPower() const { return m_power; }
    void SetPower(int8_t power) { m_power = power; }

    uint8_t GetPsduLength() const { return m_length; }
    void SetPsduLength(uint8_t length) { m_length = length; }

    uint8_t *GetPsdu() { return m_psdu; }
    uint8_t *GetHeader();
    uint8_t *GetPayload();
    uint8_t *GetFooter();

private:
    uint8_t *FindSequence();
    uint8_t *FindDstPanId();
    uint8_t *FindDstAddr();
    uint8_t *FindSrcPanId();
    uint8_t *FindSrcAddr();
    uint8_t *FindSecurityHeader();
};

}  // namespace Mac
}  // namespace Thread

#endif  // MAC_MAC_FRAME_H_
