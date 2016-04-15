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

#ifndef LOWPAN_HPP_
#define LOWPAN_HPP_

#include <common/message.hpp>
#include <mac/mac_frame.hpp>
#include <net/ip6.hpp>
#include <net/ip6_address.hpp>

namespace Thread {

namespace NetworkData { class Leader; }

class ThreadNetif;

struct Context
{
    const uint8_t *mPrefix;
    uint8_t mPrefixLength;
    uint8_t mContextId;
};

class Lowpan
{
public:
    enum
    {
        kHopsLeft = 15,
    };

    enum
    {
        kHcDispatch           = 3 << 13,
        kHcDispatchMask       = 7 << 13,
    };

    explicit Lowpan(ThreadNetif &netif);
    int Compress(Message &message, const Mac::Address &macsrc, const Mac::Address &macdst, uint8_t *buf);
    int CompressExtensionHeader(Message &message, uint8_t *buf, uint8_t &nextHeader);
    int CompressSourceIid(const Mac::Address &macaddr, const Ip6Address &ipaddr, const Context &context,
                          uint16_t &hcCtl, uint8_t *buf);
    int CompressDestinationIid(const Mac::Address &macaddr, const Ip6Address &ipaddr, const Context &context,
                               uint16_t &hcCtl, uint8_t *buf);
    int CompressMulticast(const Ip6Address &ipaddr, uint16_t &hcCtl, uint8_t *buf);

    int CompressUdp(Message &message, uint8_t *buf);

    int Decompress(Message &message, const Mac::Address &macsrc, const Mac::Address &macdst,
                   const uint8_t *buf, uint16_t bufLen, uint16_t datagramLen);
    int DecompressBaseHeader(Ip6Header &header, const Mac::Address &macsrc, const Mac::Address &macdst,
                             const uint8_t *buf);
    int DecompressExtensionHeader(Message &message, const uint8_t *buf, uint16_t bufLength);
    int DecompressUdpHeader(Message &message, const uint8_t *buf, uint16_t bufLength, uint16_t datagramLength);
    ThreadError DispatchToNextHeader(uint8_t dispatch, IpProto &nextHeader);

private:
    enum
    {
        kHcTrafficClass    = 1 << 11,
        kHcFlowLabel       = 2 << 11,
        kHcTrafficFlow     = 3 << 11,
        kHcTrafficFlowMask = 3 << 11,
        kHcNextHeader      = 1 << 10,
        kHcHopLimit1       = 1 << 8,
        kHcHopLimit64      = 2 << 8,
        kHcHopLimit255     = 3 << 8,
        kHcHopLimitMask    = 3 << 8,
        kHcContextId       = 1 << 7,
        kHcSrcAddrContext  = 1 << 6,
        kHcSrcAddrMode0    = 0 << 4,
        kHcSrcAddrMode1    = 1 << 4,
        kHcSrcAddrMode2    = 2 << 4,
        kHcSrcAddrMode3    = 3 << 4,
        kHcSrcAddrModeMask = 3 << 4,
        kHcMulticast       = 1 << 3,
        kHcDstAddrContext  = 1 << 2,
        kHcDstAddrMode0    = 0 << 0,
        kHcDstAddrMode1    = 1 << 0,
        kHcDstAddrMode2    = 2 << 0,
        kHcDstAddrMode3    = 3 << 0,
        kHcDstAddrModeMask = 3 << 0,

        kExtHdrDispatch     = 0xe0,
        kExtHdrDispatchMask = 0xf0,

        kExtHdrEidHbh       = 0x00,
        kExtHdrEidRouting   = 0x02,
        kExtHdrEidFragment  = 0x04,
        kExtHdrEidDst       = 0x06,
        kExtHdrEidMobility  = 0x08,
        kExtHdrEidIp6       = 0x0e,
        kExtHdrEidMask      = 0x0e,

        kExtHdrNextHeader   = 0x01,

        kUdpDispatch       = 0xf0,
        kUdpDispatchMask   = 0xf8,
        kUdpChecksum       = 1 << 2,
        kUdpPortMask       = 3 << 0,
    };

    NetworkData::Leader *mNetworkData;
};

class MeshHeader
{
public:
    enum
    {
        kDispatch         = 2 << 6,
        kDispatchMask     = 3 << 6,
        kHopsLeftMask     = 0xf << 0,
        kSourceShort      = 1 << 5,
        kDestinationShort = 1 << 4,
    };

    void Init() { mDispatchHopsLeft = kDispatch | kSourceShort | kDestinationShort; }
    bool IsValid() { return (mDispatchHopsLeft & kSourceShort) && (mDispatchHopsLeft & kDestinationShort); }

    uint8_t GetHeaderLength() { return sizeof(*this); }

    uint8_t GetHopsLeft() { return mDispatchHopsLeft & kHopsLeftMask; }
    void SetHopsLeft(uint8_t hops) { mDispatchHopsLeft = (mDispatchHopsLeft & ~kHopsLeftMask) | hops; }

    uint16_t GetSource() { return HostSwap16(mSource); }
    void SetSource(uint16_t source) { mSource = HostSwap16(source); }

    uint16_t GetDestination() { return HostSwap16(mDestination); }
    void SetDestination(uint16_t destination) { mDestination = HostSwap16(destination); }

private:
    uint8_t mDispatchHopsLeft;
    uint16_t mSource;
    uint16_t mDestination;
} __attribute__((packed));

class FragmentHeader
{
public:
    enum
    {
        kDispatch     = 3 << 6,
        kDispatchMask = 3 << 6,
        kOffset       = 1 << 5,
        kSizeMask     = 7 << 0,
    };

    void Init() {
        mDispatchOffsetSize = kDispatch;
    }

    uint8_t GetHeaderLength() {
        return (mDispatchOffsetSize & kOffset) ? sizeof(*this) : sizeof(*this) - sizeof(mOffset);
    }

    uint16_t GetSize() {
        return (static_cast<uint16_t>(mDispatchOffsetSize & kSizeMask) << 8) | mSize;
    }

    void SetSize(uint16_t size) {
        mDispatchOffsetSize = (mDispatchOffsetSize & ~kSizeMask) | ((size >> 8) & kSizeMask);
        mSize = size;
    }

    uint16_t GetTag() {
        return HostSwap16(mTag);
    }

    void SetTag(uint16_t tag) {
        mTag = HostSwap16(tag);
    }

    uint16_t GetOffset() {
        return (mDispatchOffsetSize & kOffset) ? static_cast<uint16_t>(mOffset) * 8 : 0;
    }

    void SetOffset(uint16_t offset) {
        if (offset == 0) {
            mDispatchOffsetSize &= ~kOffset;
        }
        else {
            mDispatchOffsetSize |= kOffset;
            mOffset = offset / 8;
        }
    }

private:
    uint8_t mDispatchOffsetSize;
    uint8_t mSize;
    uint16_t mTag;
    uint8_t mOffset;
} __attribute__((packed));

}  // namespace Thread

#endif  // LOWPAN_HPP_
