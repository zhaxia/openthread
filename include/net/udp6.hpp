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

#ifndef UDP6_HPP_
#define UDP6_HPP_

#include <net/ip6.hpp>

namespace Thread {

class Udp6Socket: private Socket
{
    friend class Udp6;

public:
    typedef void (*ReceiveHandler)(void *context, Message &message, const Ip6MessageInfo &message_info);

    Udp6Socket(ReceiveHandler handler, void *context);
    ThreadError Bind(const struct sockaddr_in6 *address);
    ThreadError Close();
    ThreadError SendTo(Message &message, const Ip6MessageInfo &message_info);

private:
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info) {
        m_handler(m_context, message, message_info);
    }

    ReceiveHandler m_handler;
    void *m_context;
    Udp6Socket *m_next;
};

class Udp6
{
public:
    static Message *NewMessage(uint16_t reserved);
    static ThreadError HandleMessage(Message &message, Ip6MessageInfo &message_info);
    static ThreadError UpdateChecksum(Message &message, uint16_t pseudoheader_checksum);
};

class UdpHeader
{
public:
    uint16_t GetSourcePort() const { return HostSwap16(m_source); }
    void SetSourcePort(uint16_t port) { m_source = HostSwap16(port); }

    uint16_t GetDestinationPort() const { return HostSwap16(m_destination); }
    void SetDestinationPort(uint16_t port) { m_destination = HostSwap16(port); }

    uint16_t GetLength() const { return HostSwap16(m_length); }
    void SetLength(uint16_t length) { m_length = HostSwap16(length); }

    uint16_t GetChecksum() const { return HostSwap16(m_checksum); }
    void SetChecksum(uint16_t checksum) { m_checksum = HostSwap16(checksum); }

    static uint8_t GetLengthOffset() { return offsetof(UdpHeader, m_length); }
    static uint8_t GetChecksumOffset() { return offsetof(UdpHeader, m_checksum); }

private:
    uint16_t m_source;
    uint16_t m_destination;
    uint16_t m_length;
    uint16_t m_checksum;
} __attribute__((packed));

}  // namespace Thread

#endif  // NET_UDP6_HPP_
