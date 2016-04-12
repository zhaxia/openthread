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

#ifndef MLE_TLVS_H_
#define MLE_TLVS_H_

#include <common/encoding.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6_address.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {
namespace Mle {

class Tlv
{
public:
    enum Type
    {
        kSourceAddress       = 0,
        kMode                = 1,
        kTimeout             = 2,
        kChallenge           = 3,
        kResponse            = 4,
        kLinkFrameCounter    = 5,
        kLinkQuality         = 6,
        kNetworkParameter    = 7,
        kMleFrameCounter     = 8,
        kRoute               = 9,
        kAddress16           = 10,
        kLeaderData          = 11,
        kNetworkData         = 12,
        kTlvRequest          = 13,
        kScanMask            = 14,
        kConnectivity        = 15,
        kLinkMargin          = 16,
        kStatus              = 17,
        kVersion             = 18,
        kAddressRegistration = 19,
        kInvalid             = 255,
    };
    Type GetType() const { return static_cast<Type>(m_type); }
    void SetType(Type type) { m_type = static_cast<uint8_t>(type); }

    uint8_t GetLength() const { return m_length; }
    void SetLength(uint8_t length) { m_length = length; }

    static ThreadError GetTlv(const Message &message, Type type, uint16_t max_length, Tlv &tlv);

private:
    uint8_t m_type;
    uint8_t m_length;
} __attribute__((packed));

class SourceAddressTlv: public Tlv
{
public:
    void Init() { SetType(kSourceAddress); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint16_t GetRloc16() const { return HostSwap16(m_rloc16); }
    void SetRloc16(uint16_t rloc16) { m_rloc16 = HostSwap16(rloc16); }

private:
    uint16_t m_rloc16;
} __attribute__((packed));

class ModeTlv: public Tlv
{
public:
    void Init() { SetType(kMode); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    enum
    {
        kModeRxOnWhenIdle      = 1 << 3,
        kModeSecureDataRequest = 1 << 2,
        kModeFFD               = 1 << 1,
        kModeFullNetworkData   = 1 << 0,
    };
    uint8_t GetMode() const { return m_mode; }
    void SetMode(uint8_t mode) { m_mode = mode; }

private:
    uint8_t m_mode;
} __attribute__((packed));

class TimeoutTlv: public Tlv
{
public:
    void Init() { SetType(kTimeout); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetTimeout() const { return HostSwap32(m_timeout); }
    void SetTimeout(uint32_t timeout) { m_timeout = HostSwap32(timeout); }

private:
    uint32_t m_timeout;
} __attribute__((packed));

class ChallengeTlv: public Tlv
{
public:
    void Init() { SetType(kChallenge); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() >= 4 && GetLength() <= 8; }

    const uint8_t *GetChallenge() const { return m_challenge; }
    void SetChallenge(const uint8_t *challenge) { memcpy(m_challenge, challenge, GetLength()); }

private:
    uint8_t m_challenge[8];
} __attribute__((packed));

class ResponseTlv: public Tlv
{
public:
    void Init() { SetType(kResponse); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    const uint8_t *GetResponse() const { return m_response; }
    void SetResponse(const uint8_t *response) { memcpy(m_response, response, GetLength()); }

private:
    uint8_t m_response[8];
} __attribute__((packed));

class LinkFrameCounterTlv: public Tlv
{
public:
    void Init() { SetType(kLinkFrameCounter); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetFrameCounter() const { return HostSwap32(m_frame_counter); }
    void SetFrameCounter(uint32_t frame_counter) { m_frame_counter = HostSwap32(frame_counter); }

private:
    uint32_t m_frame_counter;
} __attribute__((packed));

class RouteTlv: public Tlv
{
public:
    void Init() { SetType(kRoute); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const {
        return GetLength() >= sizeof(m_router_id_sequence) + sizeof(m_router_id_mask) &&
               GetLength() <= sizeof(*this) - sizeof(Tlv);
    }

    uint8_t GetRouterIdSequence() const { return m_router_id_sequence; }
    void SetRouterIdSequence(uint8_t sequence) { m_router_id_sequence = sequence; }

    void ClearRouterIdMask() { memset(m_router_id_mask, 0, sizeof(m_router_id_mask)); }
    bool IsRouterIdSet(uint8_t id) const { return (m_router_id_mask[id / 8] & (0x80 >> (id % 8))) != 0; }
    void SetRouterId(uint8_t id) { m_router_id_mask[id / 8] |= 0x80 >> (id % 8); }

    uint8_t GetRouteDataLength() const {
        return GetLength() - sizeof(m_router_id_sequence) - sizeof(m_router_id_mask);
    }

    void SetRouteDataLength(uint8_t length) {
        SetLength(sizeof(m_router_id_sequence) + sizeof(m_router_id_mask) + length);
    }

    uint8_t GetRouteCost(uint8_t i) const {
        return m_route_data[i] & kRouteCostMask;
    }

    void SetRouteCost(uint8_t i, uint8_t route_cost) {
        m_route_data[i] = (m_route_data[i] & ~kRouteCostMask) | route_cost;
    }

    uint8_t GetLinkQualityIn(uint8_t i) const {
        return (m_route_data[i] & kLinkQualityInMask) >> kLinkQualityInOffset;
    }

    void SetLinkQualityIn(uint8_t i, uint8_t link_quality) {
        m_route_data[i] = (m_route_data[i] & ~kLinkQualityInMask) | (link_quality << kLinkQualityInOffset);
    }

    uint8_t GetLinkQualityOut(uint8_t i) const {
        return (m_route_data[i] & kLinkQualityOutMask) >> kLinkQualityOutOffset;
    }

    void SetLinkQualityOut(uint8_t i, uint8_t link_quality) {
        m_route_data[i] = (m_route_data[i] & ~kLinkQualityOutMask) | (link_quality << kLinkQualityOutOffset);
    }

private:
    enum
    {
        kLinkQualityOutOffset = 6,
        kLinkQualityOutMask = 3 << kLinkQualityOutOffset,
        kLinkQualityInOffset = 4,
        kLinkQualityInMask = 3 << kLinkQualityInOffset,
        kRouteCostOffset = 0,
        kRouteCostMask = 0xf << kRouteCostOffset,
    };
    uint8_t m_router_id_sequence;
    uint8_t m_router_id_mask[8];
    uint8_t m_route_data[32];
} __attribute__((packed));

class MleFrameCounterTlv: public Tlv
{
public:
    void Init() { SetType(kMleFrameCounter); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetFrameCounter() const { return HostSwap32(m_frame_counter); }
    void SetFrameCounter(uint32_t frame_counter) { m_frame_counter = HostSwap32(frame_counter); }

private:
    uint32_t m_frame_counter;
} __attribute__((packed));

class Address16Tlv: public Tlv
{
public:
    void Init() { SetType(kAddress16); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint16_t GetRloc16() const { return HostSwap16(m_rloc16); }
    void SetRloc16(uint16_t rloc16) { m_rloc16 = HostSwap16(rloc16); }

private:
    uint16_t m_rloc16;
} __attribute__((packed));

class LeaderDataTlv: public Tlv
{
public:
    void Init() { SetType(kLeaderData); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetPartitionId() const { return HostSwap32(m_partition_id); }
    void SetPartitionId(uint32_t partition_id) { m_partition_id = HostSwap32(partition_id); }

    uint8_t GetWeighting() const { return m_weighting; }
    void SetWeighting(uint8_t weighting) { m_weighting = weighting; }

    uint8_t GetDataVersion() const { return m_data_version; }
    void SetDataVersion(uint8_t version)  { m_data_version = version; }

    uint8_t GetStableDataVersion() const { return m_stable_data_version; }
    void SetStableDataVersion(uint8_t version) { m_stable_data_version = version; }

    uint8_t GetRouterId() const { return m_router_id; }
    void SetRouterId(uint8_t router_id) { m_router_id = router_id; }

private:
    uint32_t m_partition_id;
    uint8_t m_weighting;
    uint8_t m_data_version;
    uint8_t m_stable_data_version;
    uint8_t m_router_id;
} __attribute__((packed));

class NetworkDataTlv: public Tlv
{
public:
    void Init() { SetType(kNetworkData); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() <= sizeof(*this) - sizeof(Tlv); }

    uint8_t *GetNetworkData() { return m_network_data; }
    void SetNetworkData(const uint8_t *network_data) { memcpy(m_network_data, network_data, GetLength()); }

private:
    uint8_t m_network_data[255];
} __attribute__((packed));

class TlvRequestTlv: public Tlv
{
public:
    void Init() { SetType(kTlvRequest); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() <= sizeof(*this) - sizeof(Tlv); }

    const uint8_t *GetTlvs() const { return m_tlvs; }
    void SetTlvs(const uint8_t *tlvs) { memcpy(m_tlvs, tlvs, GetLength()); }

private:
    uint8_t m_tlvs[8];
} __attribute__((packed));

class ScanMaskTlv: public Tlv
{
public:
    void Init() { SetType(kScanMask); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    enum
    {
        kRouterFlag = 1 << 7,
        kChildFlag = 1 << 6,
    };

    void ClearRouterFlag() { m_mask &= ~kRouterFlag; }
    void SetRouterFlag() { m_mask |= kRouterFlag; }
    bool IsRouterFlagSet() { return (m_mask & kRouterFlag) != 0; }

    void ClearChildFlag() { m_mask &= ~kChildFlag; }
    void SetChildFlag() { m_mask |= kChildFlag; }
    bool IsChildFlagSet() { return (m_mask & kChildFlag) != 0; }

    void SetMask(uint8_t mask) { m_mask = mask; }

private:
    uint8_t m_mask;
} __attribute__((packed));

class ConnectivityTlv: public Tlv
{
public:
    void Init() { SetType(kConnectivity); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint8_t GetMaxChildCount() const { return m_max_child_count; }
    void SetMaxChildCount(uint8_t count) { m_max_child_count = count; }

    uint8_t GetChildCount() const { return m_child_count; }
    void SetChildCount(uint8_t count) { m_child_count = count; }

    uint8_t GetLinkQuality3() const { return m_link_quality_3; }
    void SetLinkQuality3(uint8_t link_quality) { m_link_quality_3 = link_quality; }

    uint8_t GetLinkQuality2() const { return m_link_quality_2; }
    void SetLinkQuality2(uint8_t link_quality) { m_link_quality_2 = link_quality; }

    uint8_t GetLinkQuality1() const { return m_link_quality_1; }
    void SetLinkQuality1(uint8_t link_quality) { m_link_quality_1 = link_quality; }

    uint8_t GetLeaderCost() const { return m_leader_cost; }
    void SetLeaderCost(uint8_t cost) { m_leader_cost = cost; }

    uint8_t GetRouterIdSequence() const { return m_router_id_sequence; }
    void SetRouterIdSequence(uint8_t sequence) { m_router_id_sequence = sequence; }

private:
    uint8_t m_max_child_count;
    uint8_t m_child_count;
    uint8_t m_link_quality_3;
    uint8_t m_link_quality_2;
    uint8_t m_link_quality_1;
    uint8_t m_leader_cost;
    uint8_t m_router_id_sequence;
}  __attribute__((packed));

class LinkMarginTlv: public Tlv
{
public:
    void Init() { SetType(kLinkMargin); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint8_t GetLinkMargin() const { return m_link_margin; }
    void SetLinkMargin(uint8_t link_margin) { m_link_margin = link_margin; }

private:
    uint8_t m_link_margin;
} __attribute__((packed));

class StatusTlv: public Tlv
{
public:
    void Init() { SetType(kStatus); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    enum Status
    {
        kError = 1,
    };
    Status GetStatus() const { return static_cast<Status>(m_status); }
    void SetStatus(Status status) { m_status = static_cast<uint8_t>(status); }

private:
    uint8_t m_status;
} __attribute__((packed));

class VersionTlv: public Tlv
{
public:
    void Init() { SetType(kVersion); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint16_t GetVersion() const { return HostSwap16(m_version); }
    void SetVersion(uint16_t version) { m_version = HostSwap16(version); }

private:
    uint16_t m_version;
} __attribute__((packed));

class AddressRegistrationEntry
{
public:
    uint8_t GetLength() const { return sizeof(m_control) + (IsCompressed() ? sizeof(m_iid) : sizeof(m_ip6_address)); }

    bool IsCompressed() const { return m_control & kCompressed; }
    void SetUncompressed() { m_control = 0; }

    uint8_t GetContextId() const { return m_control & kCidMask; }
    void SetContextId(uint8_t cid) { m_control = kCompressed | cid; }

    const uint8_t *GetIid() const { return m_iid; }
    void SetIid(const uint8_t *iid) { memcpy(m_iid, iid, sizeof(m_iid)); }

    const Ip6Address *GetIp6Address() const { return &m_ip6_address; }
    void SetIp6Address(const Ip6Address &address) { m_ip6_address = address; }

private:
    enum
    {
        kCompressed = 1 << 7,
        kCidMask = 0xf,
    };

    uint8_t m_control;
    union
    {
        uint8_t m_iid[8];
        Ip6Address m_ip6_address;
    };
} __attribute__((packed));

class AddressRegistrationTlv: public Tlv
{
public:
    void Init() { SetType(kAddressRegistration); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() <= sizeof(*this) - sizeof(Tlv); }

    const AddressRegistrationEntry *GetAddressEntry(uint8_t index) const {
        const AddressRegistrationEntry *entry = NULL;
        const uint8_t *cur = reinterpret_cast<const uint8_t *>(m_addresses);
        const uint8_t *end = cur + GetLength();

        while (cur < end) {
            entry = reinterpret_cast<const AddressRegistrationEntry *>(cur);

            if (index == 0) {
                break;
            }

            cur += entry->GetLength();
            index--;
        }

        return entry;
    }

private:
    AddressRegistrationEntry m_addresses[4];
} __attribute__((packed));

}  // namespace Mle
}  // namespace Thread

#endif  // MLE_TLVS_H_
