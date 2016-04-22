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
 *   This file contains definitions for ICMPv6.
 */

#ifndef ICMP6_HPP_
#define ICMP6_HPP_

#include <common/encoding.hpp>
#include <net/ip6.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

class Icmp6Header
{
public:
    void Init() { mType = 0; mCode = 0; mChecksum = 0; mData.m32[0] = 0; }

    enum Type
    {
        kTypeDstUnreach = 0,
        kTypeEchoRequest = 128,
        kTypeEchoReply = 129,
    };
    Type GetType() const { return static_cast<Type>(mType); }
    void SetType(Type type) { mType = static_cast<uint8_t>(type); }

    enum Code
    {
        kCodeDstUnreachNoRoute = 0,
    };
    Code GetCode() const { return static_cast<Code>(mCode); }
    void SetCode(Code code) { mCode = static_cast<uint8_t>(code); }

    uint16_t GetChecksum() const { return HostSwap16(mChecksum); }
    void SetChecksum(uint16_t checksum) { mChecksum = HostSwap16(checksum); }

    uint16_t GetId() const { return HostSwap16(mData.m16[0]); }
    void SetId(uint16_t id) { mData.m16[0] = HostSwap16(id); }

    uint16_t GetSequence() const { return HostSwap16(mData.m16[1]); }
    void SetSequence(uint16_t sequence) { mData.m16[1] = HostSwap16(sequence); }

    static uint8_t GetChecksumOffset() { return offsetof(Icmp6Header, mChecksum); }
    static uint8_t GetDataOffset() { return offsetof(Icmp6Header, mData); }

private:
    uint8_t mType;
    uint8_t mCode;
    uint16_t mChecksum;
    union
    {
        uint32_t m32[1];
        uint16_t m16[2];
        uint8_t m8[4];
    } mData;
} __attribute__((packed));

class Icmp6Echo
{
    friend class Icmp6;

public:
    typedef void (*EchoReplyHandler)(void *context, Message &message, const Ip6MessageInfo &messageInfo);
    Icmp6Echo(EchoReplyHandler handler, void *context);
    ThreadError SendEchoRequest(const SockAddr &address, const void *payload, uint16_t payloadLength);

private:
    void HandleEchoReply(Message &message, const Ip6MessageInfo &messageInfo) {
        mHandler(mContext, message, messageInfo);
    }

    EchoReplyHandler mHandler;
    void *mContext;
    uint16_t mId;
    uint16_t mSeq;
    Icmp6Echo *mNext;

    static uint16_t sNextId;
    static Icmp6Echo *sEchoClients;
};

class Icmp6Handler
{
    friend class Icmp6;

public:
    typedef void (*DstUnreachHandler)(void *context, Message &mesage, const Ip6MessageInfo &messageInfo,
                                      const Icmp6Header &icmp6Header);
    Icmp6Handler(DstUnreachHandler dstUnreachHandler, void *context) {
        mDstUnreachHandler = dstUnreachHandler;
        mContext = context;
    }

private:
    void HandleDstUnreach(Message &message, const Ip6MessageInfo &messageInfo, const Icmp6Header &icmp6Header) {
        mDstUnreachHandler(mContext, message, messageInfo, icmp6Header);
    }

    DstUnreachHandler mDstUnreachHandler;
    void *mContext;
    Icmp6Handler *mNext;

    static Icmp6Handler *sHandlers;
};

class Icmp6
{
public:
    static ThreadError RegisterCallbacks(Icmp6Handler &handler);
    static ThreadError SendError(const Ip6Address &dst, Icmp6Header::Type type, Icmp6Header::Code code,
                                 const Ip6Header &ip6Header);
    static ThreadError HandleMessage(Message &message, Ip6MessageInfo &messageInfo);
    static ThreadError UpdateChecksum(Message &message, uint16_t pseudoheaderChecksum);

private:
    static ThreadError HandleDstUnreach(Message &message, const Ip6MessageInfo &messageInfo,
                                        const Icmp6Header &icmp6Header);
    static ThreadError HandleEchoRequest(Message &message, const Ip6MessageInfo &messageInfo);
    static ThreadError HandleEchoReply(Message &message, const Ip6MessageInfo &messageInfo,
                                       const Icmp6Header &icmp6Header);
};

}  // namespace Thread

#endif  // NET_ICMP6_HPP_
