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

#ifndef ICMP6_H_
#define ICMP6_H_

#include <common/encoding.h>
#include <net/ip6.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

class Icmp6Header
{
public:
    void Init() { m_type = 0; m_code = 0; m_checksum = 0; m_data.d32[0] = 0; }

    enum Type
    {
        kTypeDstUnreach = 0,
        kTypeEchoRequest = 128,
        kTypeEchoReply = 129,
    };
    Type GetType() const { return static_cast<Type>(m_type); }
    void SetType(Type type) { m_type = static_cast<uint8_t>(type); }

    enum Code
    {
        kCodeDstUnreachNoRoute = 0,
    };
    Code GetCode() const { return static_cast<Code>(m_code); }
    void SetCode(Code code) { m_code = static_cast<uint8_t>(code); }

    uint16_t GetChecksum() const { return HostSwap16(m_checksum); }
    void SetChecksum(uint16_t checksum) { m_checksum = HostSwap16(checksum); }

    uint16_t GetId() const { return HostSwap16(m_data.d16[0]); }
    void SetId(uint16_t id) { m_data.d16[0] = HostSwap16(id); }

    uint16_t GetSequence() const { return HostSwap16(m_data.d16[1]); }
    void SetSequence(uint16_t sequence) { m_data.d16[1] = HostSwap16(sequence); }

    static uint8_t GetChecksumOffset() { return offsetof(Icmp6Header, m_checksum); }
    static uint8_t GetDataOffset() { return offsetof(Icmp6Header, m_data); }

private:
    uint8_t m_type;
    uint8_t m_code;
    uint16_t m_checksum;
    union
    {
        uint32_t d32[1];
        uint16_t d16[2];
        uint8_t d8[4];
    } m_data;
} __attribute__((packed));

class Icmp6Echo
{
    friend class Icmp6;

public:
    typedef void (*EchoReplyHandler)(void *context, Message &message, const Ip6MessageInfo &message_info);
    Icmp6Echo(EchoReplyHandler handler, void *context);
    ThreadError SendEchoRequest(const struct sockaddr_in6 &address, const void *payload, uint16_t payload_length);

private:
    void HandleEchoReply(Message &message, const Ip6MessageInfo &message_info) {
        m_handler(m_context, message, message_info);
    }

    EchoReplyHandler m_handler;
    void *m_context;
    uint16_t m_id;
    uint16_t m_seq;
    Icmp6Echo *m_next;

    static uint16_t s_next_id;
    static Icmp6Echo *s_echo_clients;
};

class Icmp6Handler
{
    friend class Icmp6;

public:
    typedef void (*DstUnreachHandler)(void *context, Message &mesage, const Ip6MessageInfo &message_info,
                                      const Icmp6Header &icmp6_header);
    Icmp6Handler(DstUnreachHandler dst_unreach_handler, void *context) {
        m_dst_unreach_handler = dst_unreach_handler;
        m_context = context;
    }

private:
    void HandleDstUnreach(Message &message, const Ip6MessageInfo &message_info, const Icmp6Header &icmp6_header) {
        m_dst_unreach_handler(m_context, message, message_info, icmp6_header);
    }

    DstUnreachHandler m_dst_unreach_handler;
    void *m_context;
    Icmp6Handler *m_next;

    static Icmp6Handler *s_handlers;
};

class Icmp6
{
public:
    static ThreadError RegisterCallbacks(Icmp6Handler &handler);
    static ThreadError SendError(const Ip6Address &dst, Icmp6Header::Type type, Icmp6Header::Code code,
                                 const Ip6Header &ip6_header);
    static ThreadError HandleMessage(Message &message, Ip6MessageInfo &message_info);
    static ThreadError UpdateChecksum(Message &message, uint16_t pseudoheader_checksum);

private:
    static ThreadError HandleDstUnreach(Message &message, const Ip6MessageInfo &message_info,
                                        const Icmp6Header &icmp6_header);
    static ThreadError HandleEchoRequest(Message &message, const Ip6MessageInfo &message_info);
    static ThreadError HandleEchoReply(Message &message, const Ip6MessageInfo &message_info,
                                       const Icmp6Header &icmp6_header);
};

}  // namespace Thread

#endif  // NET_ICMP6_H_
