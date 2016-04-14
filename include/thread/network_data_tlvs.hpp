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

#ifndef NETWORK_DATA_TLVS_HPP_
#define NETWORK_DATA_TLVS_HPP_

#include <string.h>

#include <common/encoding.hpp>
#include <net/ip6_address.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace NetworkData {

class NetworkDataTlv
{
public:
    void Init() { m_type = 0; m_length = 0; }

    enum Type
    {
        kTypeHasRoute = 0,
        kTypePrefix = 1,
        kTypeBorderRouter = 2,
        kTypeContext = 3,
        kTypeCommissioningData = 4,
    };
    Type GetType() const { return static_cast<Type>(m_type >> kTypeOffset); }
    void SetType(Type type) { m_type = (m_type & ~kTypeMask) | (static_cast<uint8_t>(type) << kTypeOffset); }

    uint8_t GetLength() const { return m_length; }
    void SetLength(uint8_t length) { m_length = length; }
    void AdjustLength(int diff) { m_length += diff; }

    uint8_t *GetValue() { return reinterpret_cast<uint8_t *>(this) + sizeof(NetworkDataTlv); }
    NetworkDataTlv *GetNext() {
        return reinterpret_cast<NetworkDataTlv *>(reinterpret_cast<uint8_t *>(this) + sizeof(*this) + m_length);
    }

    void ClearStable() { m_type &= ~kStableMask; }
    bool IsStable() const { return (m_type & kStableMask); }
    void SetStable() { m_type |= kStableMask; }

private:
    enum
    {
        kTypeOffset = 1,
        kTypeMask = 0x7f << kTypeOffset,
        kStableMask = 1 << 0,
    };
    uint8_t m_type;
    uint8_t m_length;
} __attribute__((packed));

class HasRouteEntry
{
public:
    enum
    {
        kPreferenceOffset = 6,
        kPreferenceMask = 3 << kPreferenceOffset,
    };

    void Init() { SetRloc(0xfffe); m_flags = 0; }

    uint16_t GetRloc() const { return HostSwap16(m_rloc); }
    void SetRloc(uint16_t rloc) { m_rloc = HostSwap16(rloc); }

    int8_t GetPreference() const { return static_cast<int8_t>(m_flags) >> kPreferenceOffset; }
    void SetPreference(int8_t prf) { m_flags = (m_flags & ~kPreferenceMask) | (prf << kPreferenceOffset); }

private:
    uint16_t m_rloc;
    uint8_t m_flags;
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
    void Init(uint8_t domain_id, uint8_t prefix_length, const uint8_t *prefix) {
        NetworkDataTlv::Init();
        SetType(kTypePrefix);
        m_domain_id = domain_id;
        m_prefix_length = prefix_length;
        memcpy(GetPrefix(), prefix, (prefix_length + 7) / 8);
        SetSubTlvsLength(0);
    }

    uint8_t GetDomainId() const { return m_domain_id; }
    uint8_t GetPrefixLength() const { return m_prefix_length; }
    uint8_t *GetPrefix() { return reinterpret_cast<uint8_t *>(this) + sizeof(*this); }

    uint8_t *GetSubTlvs() { return GetPrefix() + (m_prefix_length + 7) / 8; }
    uint8_t GetSubTlvsLength() const {
        return GetLength() - (sizeof(*this) - sizeof(NetworkDataTlv) + (m_prefix_length + 7) / 8);
    }
    void SetSubTlvsLength(int length) {
        SetLength(sizeof(*this) - sizeof(NetworkDataTlv) + (m_prefix_length + 7) / 8 + length);
    }

private:
    uint8_t m_domain_id;
    uint8_t m_prefix_length;
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

    void Init() { SetRloc(0xfffe); m_flags = 0; m_reserved = 0; }

    uint16_t GetRloc() const { return HostSwap16(m_rloc); }
    void SetRloc(uint16_t rloc) { m_rloc = HostSwap16(rloc); }

    uint8_t GetFlags() const { return m_flags & ~kPreferenceMask; }
    void SetFlags(uint8_t flags) { m_flags = (m_flags & kPreferenceMask) | (flags & ~kPreferenceMask); }

    int8_t GetPreference() const { return static_cast<int8_t>(m_flags) >> kPreferenceOffset; }
    void SetPreference(int8_t prf) { m_flags = (m_flags & ~kPreferenceMask) | (prf << kPreferenceOffset); }

    bool IsPreferred() const { return (m_flags & kPreferredFlag) != 0; }
    void ClearPreferred() { m_flags &= ~kPreferredFlag; }
    void SetPreferred() { m_flags |= kPreferredFlag; }

    bool IsValid() const { return (m_flags & kValidFlag) != 0; }
    void ClearValid() { m_flags &= ~kValidFlag; }
    void SetValid() { m_flags |= kValidFlag; }

    bool IsDhcp() const { return (m_flags & kDhcpFlag) != 0; }
    void ClearDhcp() { m_flags &= ~kDhcpFlag; }
    void SetDhcp() { m_flags |= kDhcpFlag; }

    bool IsConfigure() const { return (m_flags & kConfigureFlag) != 0; }
    void ClearConfigure() { m_flags &= ~kConfigureFlag; }
    void SetConfigure() { m_flags |= kConfigureFlag; }

    bool IsDefaultRoute() const { return (m_flags & kDefaultRouteFlag) != 0; }
    void ClearDefaultRoute() { m_flags &= ~kDefaultRouteFlag; }
    void SetDefaultRoute() { m_flags |= kDefaultRouteFlag; }

private:
    uint16_t m_rloc;
    uint8_t m_flags;
    uint8_t m_reserved;
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
    void Init() { NetworkDataTlv::Init(); SetType(kTypeContext); SetLength(2); m_flags = 0; m_context_length = 0; }

    bool IsCompress() const { return (m_flags & kCompressFlag) != 0; }
    void ClearCompress() { m_flags &= ~kCompressFlag; }
    void SetCompress() { m_flags |= kCompressFlag; }

    uint8_t GetContextId() const { return m_flags & kContextIdMask; }
    void SetContextId(uint8_t cid) { m_flags = (m_flags & ~kContextIdMask) | (cid << kContextIdOffset); }

    uint8_t GetContextLength() const { return m_context_length; }
    void SetContextLength(uint8_t length) { m_context_length = length; }

private:
    enum
    {
        kCompressFlag = 1 << 4,
        kContextIdOffset = 0,
        kContextIdMask = 0xf << kContextIdOffset,
    };
    uint8_t m_flags;
    uint8_t m_context_length;
} __attribute__((packed));

}  // namespace NetworkData
}  // namespace Thread

#endif  // NETWORK_DATA_TLVS_HPP_
