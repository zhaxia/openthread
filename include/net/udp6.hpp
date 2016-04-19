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

#include <openthread.h>
#include <net/ip6.hpp>

namespace Thread {

class Udp6Socket: public otUdp6Socket
{
    friend class Udp6;

public:
    ThreadError Open(otUdp6Receive handler, void *context);
    ThreadError Bind(const SockAddr &sockname);
    ThreadError Close();
    ThreadError SendTo(Message &message, const Ip6MessageInfo &messageInfo);

    Udp6Socket *GetNext() { return static_cast<Udp6Socket *>(mNext); }
    void SetNext(Udp6Socket *socket) { mNext = static_cast<otUdp6Socket *>(socket); }

    SockAddr &GetSockName() { return *static_cast<SockAddr *>(&mSockName); }
    SockAddr &GetPeerName() { return *static_cast<SockAddr *>(&mPeerName); }

private:
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &messageInfo) {
        mHandler(mContext, &message, &messageInfo);
    }
};

class Udp6
{
public:
    static Message *NewMessage(uint16_t reserved);
    static ThreadError HandleMessage(Message &message, Ip6MessageInfo &messageInfo);
    static ThreadError UpdateChecksum(Message &message, uint16_t pseudoheaderChecksum);
};

class UdpHeader
{
public:
    uint16_t GetSourcePort() const { return HostSwap16(mSource); }
    void SetSourcePort(uint16_t port) { mSource = HostSwap16(port); }

    uint16_t GetDestinationPort() const { return HostSwap16(mDestination); }
    void SetDestinationPort(uint16_t port) { mDestination = HostSwap16(port); }

    uint16_t GetLength() const { return HostSwap16(mLength); }
    void SetLength(uint16_t length) { mLength = HostSwap16(length); }

    uint16_t GetChecksum() const { return HostSwap16(mChecksum); }
    void SetChecksum(uint16_t checksum) { mChecksum = HostSwap16(checksum); }

    static uint8_t GetLengthOffset() { return offsetof(UdpHeader, mLength); }
    static uint8_t GetChecksumOffset() { return offsetof(UdpHeader, mChecksum); }

private:
    uint16_t mSource;
    uint16_t mDestination;
    uint16_t mLength;
    uint16_t mChecksum;
} __attribute__((packed));

}  // namespace Thread

#endif  // NET_UDP6_HPP_
