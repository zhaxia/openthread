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

/**
 * @file
 *   This file contains definitions for IPv6 packet processing.
 */

#ifndef IP6_HPP_
#define IP6_HPP_

#include <common/encoding.hpp>
#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/ip6_address.hpp>
#include <net/netif.hpp>
#include <net/socket.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

class Ncp;

enum IpProto
{
    kProtoHopOpts = 0,
    kProtoUdp = 17,
    kProtoIp6 = 41,
    kProtoRouting = 43,
    kProtoFragment = 44,
    kProtoIcmp6 = 58,
    kProtoNone = 59,
    kProtoDstOpts = 60,
};

class Ip6Header
{
public:
    void Init() { mVersionClassFlow.m32[0] = 0; mVersionClassFlow.m8[0] = kVersion6; }
    bool IsVersion6() const { return (mVersionClassFlow.m8[0] & kVersionMask) == kVersion6; }

    uint16_t GetPayloadLength() { return HostSwap16(mPayloadLength); }
    void SetPayloadLength(uint16_t length) { mPayloadLength = HostSwap16(length); }

    IpProto GetNextHeader() const { return static_cast<IpProto>(mNextHeader); }
    void SetNextHeader(IpProto nextHeader) { mNextHeader = static_cast<uint8_t>(nextHeader); }

    uint8_t GetHopLimit() const { return mHopLimit; }
    void SetHopLimit(uint8_t hopLimit) { mHopLimit = hopLimit; }

    Ip6Address *GetSource() { return &mSource; }
    void SetSource(const Ip6Address &source) { mSource = source; }

    Ip6Address *GetDestination() { return &mDestination; }
    void SetDestination(const Ip6Address &destination) { mDestination = destination; }

    static uint8_t GetPayloadLengthOffset() { return offsetof(Ip6Header, mPayloadLength); }
    static uint8_t GetHopLimitOffset() { return offsetof(Ip6Header, mHopLimit); }
    static uint8_t GetHopLimitSize() { return sizeof(mHopLimit); }
    static uint8_t GetDestinationOffset() { return offsetof(Ip6Header, mDestination); }

private:
    enum
    {
        kVersion6 = 0x60,
        kVersionMask = 0xf0,
    };

    union
    {
        uint32_t m32[1];
        uint16_t m16[2];
        uint8_t m8[4];
    } mVersionClassFlow;
    uint16_t mPayloadLength;
    uint8_t mNextHeader;
    uint8_t mHopLimit;
    Ip6Address mSource;
    Ip6Address mDestination;
} __attribute__((packed));

class Ip6ExtensionHeader
{
public:
    IpProto GetNextHeader() const { return static_cast<IpProto>(mNextHeader); }
    void SetNextHeader(IpProto nextHeader) { mNextHeader = static_cast<uint8_t>(nextHeader); }

    uint16_t GetLength() const { return mLength; }
    void SetLength(uint16_t length) { mLength = length; }

private:
    uint8_t mNextHeader;
    uint8_t mLength;
} __attribute__((packed));

class Ip6HopByHopHeader: public Ip6ExtensionHeader
{
} __attribute__((packed));

class Ip6OptionHeader
{
public:
    uint8_t GetType() const { return mType; }
    void SetType(uint8_t type) { mType = type; }

    enum Action
    {
        kActionSkip = 0x00,
        kActionDiscard = 0x40,
        kActionForceIcmp = 0x80,
        kActionIcmp = 0xc0,
        kActionMask = 0xc0,
    };
    Action GetAction() const { return static_cast<Action>(mType & kActionMask); }

    uint8_t GetLength() const { return mLength; }
    void SetLength(uint8_t length) { mLength = length; }

private:
    uint8_t mType;
    uint8_t mLength;
} __attribute__((packed));

class Ip6FragmentHeader
{
public:
    void Init() { mReserved = 0; mIdentification = 0; }

    IpProto GetNextHeader() const { return static_cast<IpProto>(mNextHeader); }
    void SetNextHeader(IpProto nextHeader) { mNextHeader = static_cast<uint8_t>(nextHeader); }

    uint16_t GetOffset() { return (HostSwap16(mOffsetMore) & kOffsetMask) >> kOffsetOffset; }
    void SetOffset(uint16_t offset) {
        mOffsetMore = HostSwap16((HostSwap16(mOffsetMore) & kOffsetMask) | (offset << kOffsetOffset));
    }

    bool IsMoreFlagSet() { return HostSwap16(mOffsetMore) & kMoreFlag; }
    void ClearMoreFlag() { mOffsetMore = HostSwap16(HostSwap16(mOffsetMore) & ~kMoreFlag); }
    void SetMoreFlag() { mOffsetMore = HostSwap16(HostSwap16(mOffsetMore) | kMoreFlag); }

private:
    uint8_t mNextHeader;
    uint8_t mReserved;

    enum
    {
        kOffsetOffset = 3,
        kOffsetMask = 0xfff8,
        kMoreFlag = 1,
    };
    uint16_t mOffsetMore;
    uint32_t mIdentification;
} __attribute__((packed));

class Ip6
{
public:
    enum
    {
        kDefaultHopLimit = 64,
        kMaxDatagramLength = 1500,
    };

    static Message *NewMessage(uint16_t reserved);
    static ThreadError SendDatagram(Message &message, Ip6MessageInfo &messageInfo, IpProto ipproto);
    static ThreadError HandleDatagram(Message &message, Netif *netif, uint8_t interfaceId,
                                      const void *linkMessageInfo, bool fromNcpHost);

    static uint16_t UpdateChecksum(uint16_t checksum, uint16_t val);
    static uint16_t UpdateChecksum(uint16_t checksum, const void *buf, uint16_t length);
    static uint16_t UpdateChecksum(uint16_t checksum, const Ip6Address &address);
    static uint16_t ComputePseudoheaderChecksum(const Ip6Address &src, const Ip6Address &dst,
                                                uint16_t length, IpProto proto);

    typedef void (*NcpReceivedDatagramHandler)(void *context, Message &message);
    static void SetNcpReceivedHandler(NcpReceivedDatagramHandler handler, void *context);
};

}  // namespace Thread

#endif  // NET_IP6_HPP_
