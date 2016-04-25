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
 *   This file includes definitions for generating and processing MLE TLVs.
 */

#ifndef MLE_TLVS_HPP_
#define MLE_TLVS_HPP_

#include <common/encoding.hpp>
#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/ip6_address.hpp>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {

namespace Mle {

/**
 * @addtogroup core-mle-tlvs
 *
 * @brief
 *   This module includes definitions for generating and processing MLE TLVs.
 *
 * @{
 *
 */

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
    Type GetType() const { return static_cast<Type>(mType); }
    void SetType(Type type) { mType = static_cast<uint8_t>(type); }

    uint8_t GetLength() const { return mLength; }
    void SetLength(uint8_t length) { mLength = length; }

    static ThreadError GetTlv(const Message &message, Type type, uint16_t maxLength, Tlv &tlv);

private:
    uint8_t mType;
    uint8_t mLength;
} __attribute__((packed));

class SourceAddressTlv: public Tlv
{
public:
    void Init() { SetType(kSourceAddress); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint16_t GetRloc16() const { return HostSwap16(mRloc16); }
    void SetRloc16(uint16_t rloc16) { mRloc16 = HostSwap16(rloc16); }

private:
    uint16_t mRloc16;
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
    uint8_t GetMode() const { return mMode; }
    void SetMode(uint8_t mode) { mMode = mode; }

private:
    uint8_t mMode;
} __attribute__((packed));

class TimeoutTlv: public Tlv
{
public:
    void Init() { SetType(kTimeout); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetTimeout() const { return HostSwap32(mTimeout); }
    void SetTimeout(uint32_t timeout) { mTimeout = HostSwap32(timeout); }

private:
    uint32_t mTimeout;
} __attribute__((packed));

class ChallengeTlv: public Tlv
{
public:
    void Init() { SetType(kChallenge); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() >= 4 && GetLength() <= 8; }

    const uint8_t *GetChallenge() const { return mChallenge; }
    void SetChallenge(const uint8_t *challenge) { memcpy(mChallenge, challenge, GetLength()); }

private:
    uint8_t mChallenge[8];
} __attribute__((packed));

class ResponseTlv: public Tlv
{
public:
    void Init() { SetType(kResponse); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    const uint8_t *GetResponse() const { return mResponse; }
    void SetResponse(const uint8_t *response) { memcpy(mResponse, response, GetLength()); }

private:
    uint8_t mResponse[8];
} __attribute__((packed));

class LinkFrameCounterTlv: public Tlv
{
public:
    void Init() { SetType(kLinkFrameCounter); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetFrameCounter() const { return HostSwap32(mFrameCounter); }
    void SetFrameCounter(uint32_t frameCounter) { mFrameCounter = HostSwap32(frameCounter); }

private:
    uint32_t mFrameCounter;
} __attribute__((packed));

class RouteTlv: public Tlv
{
public:
    void Init() { SetType(kRoute); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const {
        return GetLength() >= sizeof(mRouterIdSequence) + sizeof(mRouterIdMask) &&
               GetLength() <= sizeof(*this) - sizeof(Tlv);
    }

    uint8_t GetRouterIdSequence() const { return mRouterIdSequence; }
    void SetRouterIdSequence(uint8_t sequence) { mRouterIdSequence = sequence; }

    void ClearRouterIdMask() { memset(mRouterIdMask, 0, sizeof(mRouterIdMask)); }
    bool IsRouterIdSet(uint8_t id) const { return (mRouterIdMask[id / 8] & (0x80 >> (id % 8))) != 0; }
    void SetRouterId(uint8_t id) { mRouterIdMask[id / 8] |= 0x80 >> (id % 8); }

    uint8_t GetRouteDataLength() const {
        return GetLength() - sizeof(mRouterIdSequence) - sizeof(mRouterIdMask);
    }

    void SetRouteDataLength(uint8_t length) {
        SetLength(sizeof(mRouterIdSequence) + sizeof(mRouterIdMask) + length);
    }

    uint8_t GetRouteCost(uint8_t i) const {
        return mRouteData[i] & kRouteCostMask;
    }

    void SetRouteCost(uint8_t i, uint8_t routeCost) {
        mRouteData[i] = (mRouteData[i] & ~kRouteCostMask) | routeCost;
    }

    uint8_t GetLinkQualityIn(uint8_t i) const {
        return (mRouteData[i] & kLinkQualityInMask) >> kLinkQualityInOffset;
    }

    void SetLinkQualityIn(uint8_t i, uint8_t linkQuality) {
        mRouteData[i] = (mRouteData[i] & ~kLinkQualityInMask) | (linkQuality << kLinkQualityInOffset);
    }

    uint8_t GetLinkQualityOut(uint8_t i) const {
        return (mRouteData[i] & kLinkQualityOutMask) >> kLinkQualityOutOffset;
    }

    void SetLinkQualityOut(uint8_t i, uint8_t linkQuality) {
        mRouteData[i] = (mRouteData[i] & ~kLinkQualityOutMask) | (linkQuality << kLinkQualityOutOffset);
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
    uint8_t mRouterIdSequence;
    uint8_t mRouterIdMask[8];
    uint8_t mRouteData[32];
} __attribute__((packed));

class MleFrameCounterTlv: public Tlv
{
public:
    void Init() { SetType(kMleFrameCounter); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetFrameCounter() const { return HostSwap32(mFrameCounter); }
    void SetFrameCounter(uint32_t frameCounter) { mFrameCounter = HostSwap32(frameCounter); }

private:
    uint32_t mFrameCounter;
} __attribute__((packed));

class Address16Tlv: public Tlv
{
public:
    void Init() { SetType(kAddress16); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint16_t GetRloc16() const { return HostSwap16(mRloc16); }
    void SetRloc16(uint16_t rloc16) { mRloc16 = HostSwap16(rloc16); }

private:
    uint16_t mRloc16;
} __attribute__((packed));

class LeaderDataTlv: public Tlv
{
public:
    void Init() { SetType(kLeaderData); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint32_t GetPartitionId() const { return HostSwap32(mPartitionId); }
    void SetPartitionId(uint32_t partitionId) { mPartitionId = HostSwap32(partitionId); }

    uint8_t GetWeighting() const { return mWeighting; }
    void SetWeighting(uint8_t weighting) { mWeighting = weighting; }

    uint8_t GetDataVersion() const { return mDataVersion; }
    void SetDataVersion(uint8_t version)  { mDataVersion = version; }

    uint8_t GetStableDataVersion() const { return mStableDataVersion; }
    void SetStableDataVersion(uint8_t version) { mStableDataVersion = version; }

    uint8_t GetRouterId() const { return mRouterId; }
    void SetRouterId(uint8_t routerId) { mRouterId = routerId; }

private:
    uint32_t mPartitionId;
    uint8_t mWeighting;
    uint8_t mDataVersion;
    uint8_t mStableDataVersion;
    uint8_t mRouterId;
} __attribute__((packed));

class NetworkDataTlv: public Tlv
{
public:
    void Init() { SetType(kNetworkData); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() <= sizeof(*this) - sizeof(Tlv); }

    uint8_t *GetNetworkData() { return mNetworkData; }
    void SetNetworkData(const uint8_t *networkData) { memcpy(mNetworkData, networkData, GetLength()); }

private:
    uint8_t mNetworkData[255];
} __attribute__((packed));

class TlvRequestTlv: public Tlv
{
public:
    void Init() { SetType(kTlvRequest); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() <= sizeof(*this) - sizeof(Tlv); }

    const uint8_t *GetTlvs() const { return mTlvs; }
    void SetTlvs(const uint8_t *tlvs) { memcpy(mTlvs, tlvs, GetLength()); }

private:
    uint8_t mTlvs[8];
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

    void ClearRouterFlag() { mMask &= ~kRouterFlag; }
    void SetRouterFlag() { mMask |= kRouterFlag; }
    bool IsRouterFlagSet() { return (mMask & kRouterFlag) != 0; }

    void ClearChildFlag() { mMask &= ~kChildFlag; }
    void SetChildFlag() { mMask |= kChildFlag; }
    bool IsChildFlagSet() { return (mMask & kChildFlag) != 0; }

    void SetMask(uint8_t mask) { mMask = mask; }

private:
    uint8_t mMask;
} __attribute__((packed));

class ConnectivityTlv: public Tlv
{
public:
    void Init() { SetType(kConnectivity); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint8_t GetMaxChildCount() const { return mMaxChildCount; }
    void SetMaxChildCount(uint8_t count) { mMaxChildCount = count; }

    uint8_t GetChildCount() const { return mChildCount; }
    void SetChildCount(uint8_t count) { mChildCount = count; }

    uint8_t GetLinkQuality3() const { return mLinkQuality3; }
    void SetLinkQuality3(uint8_t linkQuality) { mLinkQuality3 = linkQuality; }

    uint8_t GetLinkQuality2() const { return mLinkQuality2; }
    void SetLinkQuality2(uint8_t linkQuality) { mLinkQuality2 = linkQuality; }

    uint8_t GetLinkQuality1() const { return mLinkQuality1; }
    void SetLinkQuality1(uint8_t linkQuality) { mLinkQuality1 = linkQuality; }

    uint8_t GetLeaderCost() const { return mLeaderCost; }
    void SetLeaderCost(uint8_t cost) { mLeaderCost = cost; }

    uint8_t GetRouterIdSequence() const { return mRouterIdSequence; }
    void SetRouterIdSequence(uint8_t sequence) { mRouterIdSequence = sequence; }

private:
    uint8_t mMaxChildCount;
    uint8_t mChildCount;
    uint8_t mLinkQuality3;
    uint8_t mLinkQuality2;
    uint8_t mLinkQuality1;
    uint8_t mLeaderCost;
    uint8_t mRouterIdSequence;
}  __attribute__((packed));

class LinkMarginTlv: public Tlv
{
public:
    void Init() { SetType(kLinkMargin); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint8_t GetLinkMargin() const { return mLinkMargin; }
    void SetLinkMargin(uint8_t linkMargin) { mLinkMargin = linkMargin; }

private:
    uint8_t mLinkMargin;
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
    Status GetStatus() const { return static_cast<Status>(mStatus); }
    void SetStatus(Status status) { mStatus = static_cast<uint8_t>(status); }

private:
    uint8_t mStatus;
} __attribute__((packed));

class VersionTlv: public Tlv
{
public:
    void Init() { SetType(kVersion); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(Tlv); }

    uint16_t GetVersion() const { return HostSwap16(mVersion); }
    void SetVersion(uint16_t version) { mVersion = HostSwap16(version); }

private:
    uint16_t mVersion;
} __attribute__((packed));

class AddressRegistrationEntry
{
public:
    uint8_t GetLength() const { return sizeof(mControl) + (IsCompressed() ? sizeof(mIid) : sizeof(mIp6Address)); }

    bool IsCompressed() const { return mControl & kCompressed; }
    void SetUncompressed() { mControl = 0; }

    uint8_t GetContextId() const { return mControl & kCidMask; }
    void SetContextId(uint8_t cid) { mControl = kCompressed | cid; }

    const uint8_t *GetIid() const { return mIid; }
    void SetIid(const uint8_t *iid) { memcpy(mIid, iid, sizeof(mIid)); }

    const Ip6Address *GetIp6Address() const { return &mIp6Address; }
    void SetIp6Address(const Ip6Address &address) { mIp6Address = address; }

private:
    enum
    {
        kCompressed = 1 << 7,
        kCidMask = 0xf,
    };

    uint8_t mControl;
    union
    {
        uint8_t mIid[8];
        Ip6Address mIp6Address;
    };
} __attribute__((packed));

class AddressRegistrationTlv: public Tlv
{
public:
    void Init() { SetType(kAddressRegistration); SetLength(sizeof(*this) - sizeof(Tlv)); }
    bool IsValid() const { return GetLength() <= sizeof(*this) - sizeof(Tlv); }

    const AddressRegistrationEntry *GetAddressEntry(uint8_t index) const {
        const AddressRegistrationEntry *entry = NULL;
        const uint8_t *cur = reinterpret_cast<const uint8_t *>(mAddresses);
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
    AddressRegistrationEntry mAddresses[4];
} __attribute__((packed));

/**
 * @}
 */

}  // namespace Mle


}  // namespace Thread

#endif  // MLE_TLVS_HPP_
