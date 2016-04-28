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
 *   This file includes definitions and methods for generating and processing Thread Network Layer TLVs.
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
    Type GetType() const { return static_cast<Type>(mType); }
    void SetType(Type type) { mType = static_cast<uint8_t>(type); }

    uint8_t GetLength() const { return mLength; }
    void SetLength(uint8_t length) { mLength = length; }

    static ThreadError GetTlv(const Message &message, Type type, uint16_t maxLength, ThreadTlv &tlv);

private:
    uint8_t mType;
    uint8_t mLength;
} __attribute__((packed));

class ThreadTargetTlv: public ThreadTlv
{
public:
    void Init() { SetType(kTarget); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    const Ip6::Address *GetTarget() const { return &mTarget; }
    void SetTarget(const Ip6::Address &target) { mTarget = target; }

private:
    Ip6::Address mTarget;
} __attribute__((packed));

class ThreadMacAddr64Tlv: public ThreadTlv
{
public:
    void Init() { SetType(kMacAddr64); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    const Mac::ExtAddress *GetMacAddr() const { return &mMacAddr; }
    void SetMacAddr(const Mac::ExtAddress &macaddr) { mMacAddr = macaddr; }

private:
    Mac::ExtAddress mMacAddr;
};

class ThreadRlocTlv: public ThreadTlv
{
public:
    void Init() { SetType(kRloc); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    uint16_t GetRloc16() const { return HostSwap16(mRloc16); }
    void SetRloc16(uint16_t rloc16) { mRloc16 = HostSwap16(rloc16); }

private:
    uint16_t mRloc16;
} __attribute__((packed));

class ThreadMeshLocalIidTlv: public ThreadTlv
{
public:
    void Init() { SetType(kMeshLocalIid); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    const uint8_t *GetIid() const { return mIid; }
    void SetIid(const uint8_t *iid) { memcpy(mIid, iid, sizeof(mIid)); }

private:
    uint8_t mIid[8];
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
    Status GetStatus() const { return static_cast<Status>(mStatus); }
    void SetStatus(Status status) { mStatus = static_cast<uint8_t>(status); }

private:
    uint8_t mStatus;
} __attribute__((packed));

class ThreadLastTransactionTimeTlv: public ThreadTlv
{
public:
    void Init() { SetType(kLastTransactionTime); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    uint32_t GetTime() const { return HostSwap32(mTime); }
    void SetTime(uint32_t time) { mTime = HostSwap32(time); }

private:
    uint32_t mTime;
} __attribute__((packed));

class ThreadRouterMaskTlv: public ThreadTlv
{
public:
    void Init() { SetType(kRouterMask); SetLength(sizeof(*this) - sizeof(ThreadTlv)); }
    bool IsValid() const { return GetLength() == sizeof(*this) - sizeof(ThreadTlv); }

    uint8_t GetRouterIdSequence() const { return mRouterIdSequence; }
    void SetRouterIdSequence(uint8_t sequence) { mRouterIdSequence = sequence; }

    void ClearRouterIdMask() { memset(mRouterIdMask, 0, sizeof(mRouterIdMask)); }
    bool IsRouterIdSet(uint8_t id) const { return (mRouterIdMask[id / 8] & (0x80 >> (id % 8))) != 0; }
    void SetRouterId(uint8_t id) { mRouterIdMask[id / 8] |= 0x80 >> (id % 8); }

private:
    uint8_t mRouterIdSequence;
    uint8_t mRouterIdMask[(Mle::kMaxRouterId + 7) / 8];
};

}  // namespace Thread

#endif  // THREAD_TLVS_HPP_
