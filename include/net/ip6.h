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

#ifndef IP6_H_
#define IP6_H_

#include <common/encoding.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6_address.h>
#include <net/netif.h>
#include <net/socket.h>

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
    void Init() { m_version_class_flow.d32[0] = 0; m_version_class_flow.d8[0] = kVersion6; }
    bool IsVersion6() const { return (m_version_class_flow.d8[0] & kVersionMask) == kVersion6; }

    uint16_t GetPayloadLength() { return HostSwap16(m_payload_length); }
    void SetPayloadLength(uint16_t length) { m_payload_length = HostSwap16(length); }

    IpProto GetNextHeader() const { return static_cast<IpProto>(m_next_header); }
    void SetNextHeader(IpProto next_header) { m_next_header = static_cast<uint8_t>(next_header); }

    uint8_t GetHopLimit() const { return m_hop_limit; }
    void SetHopLimit(uint8_t hop_limit) { m_hop_limit = hop_limit; }

    Ip6Address *GetSource() { return &m_source; }
    void SetSource(const Ip6Address &source) { m_source = source; }

    Ip6Address *GetDestination() { return &m_destination; }
    void SetDestination(const Ip6Address &destination) { m_destination = destination; }

    static uint8_t GetPayloadLengthOffset() { return offsetof(Ip6Header, m_payload_length); }
    static uint8_t GetHopLimitOffset() { return offsetof(Ip6Header, m_hop_limit); }
    static uint8_t GetHopLimitSize() { return sizeof(m_hop_limit); }
    static uint8_t GetDestinationOffset() { return offsetof(Ip6Header, m_destination); }

private:
    enum
    {
        kVersion6 = 0x60,
        kVersionMask = 0xf0,
    };

    union
    {
        uint32_t d32[1];
        uint16_t d16[2];
        uint8_t d8[4];
    } m_version_class_flow;
    uint16_t m_payload_length;
    uint8_t m_next_header;
    uint8_t m_hop_limit;
    Ip6Address m_source;
    Ip6Address m_destination;
} __attribute__((packed));

class Ip6ExtensionHeader
{
public:
    IpProto GetNextHeader() const { return static_cast<IpProto>(m_next_header); }
    void SetNextHeader(IpProto next_header) { m_next_header = static_cast<uint8_t>(next_header); }

    uint16_t GetLength() const { return m_length; }
    void SetLength(uint16_t length) { m_length = length; }

private:
    uint8_t m_next_header;
    uint8_t m_length;
} __attribute__((packed));

class Ip6HopByHopHeader: public Ip6ExtensionHeader
{
} __attribute__((packed));

class Ip6OptionHeader
{
public:
    uint8_t GetType() const { return m_type; }
    void SetType(uint8_t type) { m_type = type; }

    enum Action
    {
        kActionSkip = 0x00,
        kActionDiscard = 0x40,
        kActionForceIcmp = 0x80,
        kActionIcmp = 0xc0,
        kActionMask = 0xc0,
    };
    Action GetAction() const { return static_cast<Action>(m_type & kActionMask); }

    uint8_t GetLength() const { return m_length; }
    void SetLength(uint8_t length) { m_length = length; }

private:
    uint8_t m_type;
    uint8_t m_length;
} __attribute__((packed));

class Ip6FragmentHeader
{
public:
    void Init() { m_reserved = 0; m_identification = 0; }

    IpProto GetNextHeader() const { return static_cast<IpProto>(m_next_header); }
    void SetNextHeader(IpProto next_header) { m_next_header = static_cast<uint8_t>(next_header); }

    uint16_t GetOffset() { return (HostSwap16(m_offset_more) & kOffsetMask) >> kOffsetOffset; }
    void SetOffset(uint16_t offset) {
        m_offset_more = HostSwap16((HostSwap16(m_offset_more) & kOffsetMask) | (offset << kOffsetOffset));
    }

    bool IsMoreFlagSet() { return HostSwap16(m_offset_more) & kMoreFlag; }
    void ClearMoreFlag() { m_offset_more = HostSwap16(HostSwap16(m_offset_more) & ~kMoreFlag); }
    void SetMoreFlag() { m_offset_more = HostSwap16(HostSwap16(m_offset_more) | kMoreFlag); }

private:
    uint8_t m_next_header;
    uint8_t m_reserved;

    enum
    {
        kOffsetOffset = 3,
        kOffsetMask = 0xfff8,
        kMoreFlag = 1,
    };
    uint16_t m_offset_more;
    uint32_t m_identification;
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
    static ThreadError SendDatagram(Message &message, Ip6MessageInfo &message_info, IpProto ipproto);
    static ThreadError HandleDatagram(Message &message, Netif *netif, uint8_t interface_id,
                                      const void *link_message_info, bool from_ncp_host);

    static uint16_t UpdateChecksum(uint16_t checksum, uint16_t val);
    static uint16_t UpdateChecksum(uint16_t checksum, const void *buf, uint16_t length);
    static uint16_t UpdateChecksum(uint16_t checksum, const Ip6Address &address);
    static uint16_t ComputePseudoheaderChecksum(const Ip6Address &src, const Ip6Address &dst,
                                                uint16_t length, IpProto proto);

    typedef void (*NcpReceivedDatagramHandler)(void *context, Message &message);
    static void SetNcpReceivedHandler(NcpReceivedDatagramHandler handler, void *context);
};

}  // namespace Thread

#endif  // NET_IP6_H_
