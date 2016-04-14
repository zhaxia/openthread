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

#ifndef THREAD_TLVS_HPP_
#define THREAD_TLVS_HPP_

#include <common/encoding.hpp>
#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/ip6_address.hpp>
#include <thread/mle.hpp>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {

enum
{
    kCoapUdpPort = 19789,
};

class ThreadTlv
{
public:
    enum Type
    {
        kTarget              = 0,
        kMacAddr64           = 1,
        kRloc                = 2,
        kMeshLocalIid        = 3,
        kStatus              = 4,
        kLastTransactionTime = 6,
        kRouterMask          = 7,
    };
    Type GetType() const { return static_cast<Type>(m_type); }
    void SetType(Type type) { m_type = static_cast<uint8_t>(type); }

    uint8_t GetLength() const { return m_length; }
    void SetLength(uint8_t length) { m_length = length; }

    static ThreadError GetTlv(const Message &message, Type type, uint16_t max_length, ThreadTlv &tlv);

private:
    uint8_t m_type;
    uint8_t m_length;
} __attribute__((packed));

class ThreadTargetTlv: public ThreadTlv
{
public:
    void Init() { SetType(kTarget); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    const Ip6Address *GetTarget() const { return &m_target; }
    void SetTarget(const Ip6Address &target) { m_target = target; }

private:
    Ip6Address m_target;
} __attribute__((packed));

class ThreadMacAddr64Tlv: public ThreadTlv
{
public:
    void Init() { SetType(kMacAddr64); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    const Mac::Address64 *GetMacAddr() const { return &m_mac_addr; }
    void SetMacAddr(const Mac::Address64 &macaddr) { m_mac_addr = macaddr; }

private:
    Mac::Address64 m_mac_addr;
};

class ThreadRlocTlv: public ThreadTlv
{
public:
    void Init() { SetType(kRloc); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    uint16_t GetRloc16() const { return HostSwap16(m_rloc16); }
    void SetRloc16(uint16_t rloc16) { m_rloc16 = HostSwap16(rloc16); }

private:
    uint16_t m_rloc16;
} __attribute__((packed));

class ThreadMeshLocalIidTlv: public ThreadTlv
{
public:
    void Init() { SetType(kMeshLocalIid); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    const uint8_t *GetIid() const { return m_iid; }
    void SetIid(const uint8_t *iid) { memcpy(m_iid, iid, sizeof(m_iid)); }

private:
    uint8_t m_iid[8];
} __attribute__((packed));

class ThreadStatusTlv: public ThreadTlv
{
public:
    void Init() { SetType(kStatus); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    enum Status
    {
        kSuccess = 0,
        kNoAddressAvailable = 1,
    };
    Status GetStatus() const { return static_cast<Status>(m_status); }
    void SetStatus(Status status) { m_status = static_cast<uint8_t>(status); }

private:
    uint8_t m_status;
} __attribute__((packed));

class ThreadLastTransactionTimeTlv: public ThreadTlv
{
public:
    void Init() { SetType(kLastTransactionTime); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    uint32_t GetTime() const { return HostSwap32(m_time); }
    void SetTime(uint32_t time) { m_time = HostSwap32(time); }

private:
    uint32_t m_time;
} __attribute__((packed));

class ThreadRouterMaskTlv: public ThreadTlv
{
public:
    void Init() { SetType(kRouterMask); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    uint8_t GetRouterIdSequence() const { return m_router_id_sequence; }
    void SetRouterIdSequence(uint8_t sequence) { m_router_id_sequence = sequence; }

    void ClearRouterIdMask() { memset(m_router_id_mask, 0, sizeof(m_router_id_mask)); }
    bool IsRouterIdSet(uint8_t id) const { return (m_router_id_mask[id / 8] & (0x80 >> (id % 8))) != 0; }
    void SetRouterId(uint8_t id) { m_router_id_mask[id / 8] |= 0x80 >> (id % 8); }

private:
    uint8_t m_router_id_sequence;
    uint8_t m_router_id_mask[(Mle::kMaxRouterId + 7) / 8];
};

}  // namespace Thread

#endif  // THREAD_TLVS_HPP_
