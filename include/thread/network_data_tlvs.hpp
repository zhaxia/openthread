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
 *   This file includes definitions for generating and processing Thread Network Data TLVs.
 */

#ifndef NETWORK_DATA_TLVS_HPP_
#define NETWORK_DATA_TLVS_HPP_

#include <string.h>

#include <common/encoding.hpp>
#include <net/ip6_address.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace NetworkData {

/**
 * @addtogroup core-netdata-tlvs
 *
 * @brief
 *   This module includes definitions for generating and processing Thread Network Data TLVs.
 *
 * @{
 *
 */

class NetworkDataTlv
{
public:
    void Init() { mTyle = 0; mLength = 0; }

    enum Type
    {
        kTypeHasRoute = 0,
        kTypePrefix = 1,
        kTypeBorderRouter = 2,
        kTypeContext = 3,
        kTypeCommissioningData = 4,
    };
    Type GetType() const { return static_cast<Type>(mTyle >> kTypeOffset); }
    void SetType(Type type) { mTyle = (mTyle & ~kTypeMask) | (static_cast<uint8_t>(type) << kTypeOffset); }

    uint8_t GetLength() const { return mLength; }
    void SetLength(uint8_t length) { mLength = length; }
    void AdjustLength(int diff) { mLength += diff; }

    uint8_t *GetValue() { return reinterpret_cast<uint8_t *>(this) + sizeof(NetworkDataTlv); }
    NetworkDataTlv *GetNext() {
        return reinterpret_cast<NetworkDataTlv *>(reinterpret_cast<uint8_t *>(this) + sizeof(*this) + mLength);
    }

    void ClearStable() { mTyle &= ~kStableMask; }
    bool IsStable() const { return (mTyle & kStableMask); }
    void SetStable() { mTyle |= kStableMask; }

private:
    enum
    {
        kTypeOffset = 1,
        kTypeMask = 0x7f << kTypeOffset,
        kStableMask = 1 << 0,
    };
    uint8_t mTyle;
    uint8_t mLength;
} __attribute__((packed));

class HasRouteEntry
{
public:
    enum
    {
        kPreferenceOffset = 6,
        kPreferenceMask = 3 << kPreferenceOffset,
    };

    void Init() { SetRloc(0xfffe); mFlags = 0; }

    uint16_t GetRloc() const { return HostSwap16(mRloc); }
    void SetRloc(uint16_t rloc) { mRloc = HostSwap16(rloc); }

    int8_t GetPreference() const { return static_cast<int8_t>(mFlags) >> kPreferenceOffset; }
    void SetPreference(int8_t prf) { mFlags = (mFlags & ~kPreferenceMask) | (prf << kPreferenceOffset); }

private:
    uint16_t mRloc;
    uint8_t mFlags;
} __attribute__((packed));

class HasRouteTlv: public NetworkDataTlv
{
public:
    void Init() { NetworkDataTlv::Init(); SetType(kTypeHasRoute); SetLength(0); }

    uint8_t GetNumEntries() const { return GetLength() / sizeof(HasRouteEntry); }

    HasRouteEntry *GetEntry(int i) {
        return reinterpret_cast<HasRouteEntry *>(GetValue() + (i * sizeof(HasRouteEntry)));
    }
} __attribute__((packed));

class PrefixTlv: public NetworkDataTlv
{
public:
    void Init(uint8_t domain_id, uint8_t prefixLength, const uint8_t *prefix) {
        NetworkDataTlv::Init();
        SetType(kTypePrefix);
        mDomainId = domain_id;
        mPrefixLength = prefixLength;
        memcpy(GetPrefix(), prefix, (prefixLength + 7) / 8);
        SetSubTlvsLength(0);
    }

    uint8_t GetDomainId() const { return mDomainId; }
    uint8_t GetPrefixLength() const { return mPrefixLength; }
    uint8_t *GetPrefix() { return reinterpret_cast<uint8_t *>(this) + sizeof(*this); }

    uint8_t *GetSubTlvs() { return GetPrefix() + (mPrefixLength + 7) / 8; }
    uint8_t GetSubTlvsLength() const {
        return GetLength() - (sizeof(*this) - sizeof(NetworkDataTlv) + (mPrefixLength + 7) / 8);
    }
    void SetSubTlvsLength(int length) {
        SetLength(sizeof(*this) - sizeof(NetworkDataTlv) + (mPrefixLength + 7) / 8 + length);
    }

private:
    uint8_t mDomainId;
    uint8_t mPrefixLength;
} __attribute__((packed));

class BorderRouterEntry
{
public:
    enum
    {
        kPreferenceOffset = 6,
        kPreferenceMask = 3 << kPreferenceOffset,
        kPreferredFlag = 1 << 5,
        kValidFlag = 1 << 4,
        kDhcpFlag = 1 << 3,
        kConfigureFlag = 1 << 2,
        kDefaultRouteFlag = 1 << 1,
    };

    void Init() { SetRloc(0xfffe); mFlags = 0; mReserved = 0; }

    uint16_t GetRloc() const { return HostSwap16(mRloc); }
    void SetRloc(uint16_t rloc) { mRloc = HostSwap16(rloc); }

    uint8_t GetFlags() const { return mFlags & ~kPreferenceMask; }
    void SetFlags(uint8_t flags) { mFlags = (mFlags & kPreferenceMask) | (flags & ~kPreferenceMask); }

    int8_t GetPreference() const { return static_cast<int8_t>(mFlags) >> kPreferenceOffset; }
    void SetPreference(int8_t prf) { mFlags = (mFlags & ~kPreferenceMask) | (prf << kPreferenceOffset); }

    bool IsPreferred() const { return (mFlags & kPreferredFlag) != 0; }
    void ClearPreferred() { mFlags &= ~kPreferredFlag; }
    void SetPreferred() { mFlags |= kPreferredFlag; }

    bool IsValid() const { return (mFlags & kValidFlag) != 0; }
    void ClearValid() { mFlags &= ~kValidFlag; }
    void SetValid() { mFlags |= kValidFlag; }

    bool IsDhcp() const { return (mFlags & kDhcpFlag) != 0; }
    void ClearDhcp() { mFlags &= ~kDhcpFlag; }
    void SetDhcp() { mFlags |= kDhcpFlag; }

    bool IsConfigure() const { return (mFlags & kConfigureFlag) != 0; }
    void ClearConfigure() { mFlags &= ~kConfigureFlag; }
    void SetConfigure() { mFlags |= kConfigureFlag; }

    bool IsDefaultRoute() const { return (mFlags & kDefaultRouteFlag) != 0; }
    void ClearDefaultRoute() { mFlags &= ~kDefaultRouteFlag; }
    void SetDefaultRoute() { mFlags |= kDefaultRouteFlag; }

private:
    uint16_t mRloc;
    uint8_t mFlags;
    uint8_t mReserved;
} __attribute__((packed));

class BorderRouterTlv: public NetworkDataTlv
{
public:
    void Init() { NetworkDataTlv::Init(); SetType(kTypeBorderRouter); SetLength(0); }

    uint8_t GetNumEntries() const { return GetLength() / sizeof(BorderRouterEntry); }

    BorderRouterEntry *GetEntry(int i) {
        return reinterpret_cast<BorderRouterEntry *>(GetValue() + (i * sizeof(BorderRouterEntry)));
    }
} __attribute__((packed));

class ContextTlv: public NetworkDataTlv
{
public:
    void Init() { NetworkDataTlv::Init(); SetType(kTypeContext); SetLength(2); mFlags = 0; mContextLength = 0; }

    bool IsCompress() const { return (mFlags & kCompressFlag) != 0; }
    void ClearCompress() { mFlags &= ~kCompressFlag; }
    void SetCompress() { mFlags |= kCompressFlag; }

    uint8_t GetContextId() const { return mFlags & kContextIdMask; }
    void SetContextId(uint8_t cid) { mFlags = (mFlags & ~kContextIdMask) | (cid << kContextIdOffset); }

    uint8_t GetContextLength() const { return mContextLength; }
    void SetContextLength(uint8_t length) { mContextLength = length; }

private:
    enum
    {
        kCompressFlag = 1 << 4,
        kContextIdOffset = 0,
        kContextIdMask = 0xf << kContextIdOffset,
    };
    uint8_t mFlags;
    uint8_t mContextLength;
} __attribute__((packed));

/**
 * @}
 *
 */

}  // namespace NetworkData
}  // namespace Thread

#endif  // NETWORK_DATA_TLVS_HPP_
