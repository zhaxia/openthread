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
 *   This file includes definitions for UDP/IPv6 sockets.
 */

#ifndef UDP6_HPP_
#define UDP6_HPP_

#include <openthread.h>
#include <net/ip6.hpp>

namespace Thread {
namespace Ip6 {

/**
 * @addtogroup core-udp
 *
 * @brief
 *   This module includes definitions for UDP/IPv6 sockets.
 *
 * @{
 *
 */

/**
 * This class implements a UDP/IPv6 socket.
 *
 */
class UdpSocket: public otUdpSocket
{
    friend class Udp;

public:
    /**
     * This method opens the UDP socket.
     *
     * @param[in]  aHandler  A pointer to a function that is called when receiving UDP messages.
     * @param[in]  aContext  A pointer to arbitrary context information.
     *
     * @retval kThreadError_None  Successfully opened the socket.
     * @retval kThreadError_Busy  The socket is already open.
     *
     */
    ThreadError Open(otUdpReceive aHandler, void *aContext);

    /**
     * This method binds the UDP socket.
     *
     * @param[in]  aSockAddr  A reference to the socket address.
     *
     * @retval kThreadError_None  Successfully bound the socket.
     *
     */
    ThreadError Bind(const SockAddr &aSockAddr);

    /**
     * This method closes the UDP socket.
     *
     * @retval kThreadError_None  Successfully closed the UDP socket.
     * @retval kThreadErrorBusy   The socket is already closed.
     *
     */
    ThreadError Close(void);

    /**
     * This method sends a UDP message.
     *
     * @param[in]  aMessage      The message to send.
     * @param[in]  aMessageInfo  The message info associated with @p aMessage.
     *
     * @retval kThreadError_None    Successfully sent the UDP messag.
     * @retval kThreadError_NoBufs  Insufficient available buffer to add the UDP and IPv6 headers.
     *
     */
    ThreadError SendTo(Message &aMessage, const MessageInfo &aMessageInfo);

private:
    UdpSocket *GetNext(void) { return static_cast<UdpSocket *>(mNext); }
    void SetNext(UdpSocket *socket) { mNext = static_cast<otUdpSocket *>(socket); }

    SockAddr &GetSockName(void) { return *static_cast<SockAddr *>(&mSockName); }
    SockAddr &GetPeerName(void) { return *static_cast<SockAddr *>(&mPeerName); }

    void HandleUdpReceive(Message &aMessage, const MessageInfo &aMessageInfo) {
        mHandler(mContext, &aMessage, &aMessageInfo);
    }
};

/**
 * This class implements core UDP message handling.
 *
 */
class Udp
{
public:
    /**
     * This static method returns a new UDP message with sufficient header space reserved.
     *
     * @param[in]  aReserved  The number of header bytes to reserve after the UDP header.
     *
     * @returns A pointer to the message or NULL if no buffers are available.
     *
     */
    static Message *NewMessage(uint16_t aReserved);

    /**
     * This static method handles a received UDP message.
     *
     * @param[in]  aMessage      A reference to the UDP message to process.
     * @param[in]  aMessageInfo  A reference to the message info associated with @p aMessage.
     *
     * @retval kThreadError_None  Successfully processed the UDP message.
     * @retval kThreadError_Drop  Could not fully process the UDP message.
     *
     */
    static ThreadError HandleMessage(Message &aMessage, MessageInfo &aMessageInfo);

    /**
     * This static method updates the UDP checksum.
     *
     * @param[in]  aMessage               A reference to the UDP message.
     * @param[in]  aPseudoHeaderChecksum  The pseudo-header checksum value.
     *
     * @retval kThreadError_None         Successfully updated the UDP checksum.
     * @retval kThreadError_InvalidArgs  The message was invalid.
     *
     */
    static ThreadError UpdateChecksum(Message &aMessage, uint16_t aPseudoHeaderChecksum);
};

/**
 * This class implements UDP header generation and parsing.
 *
 */
class UdpHeader
{
public:
    /**
     * This method returns the UDP Source Port.
     *
     * @returns The UDP Source Port.
     *
     */
    uint16_t GetSourcePort(void) const { return HostSwap16(mSource); }

    /**
     * This method sets the UDP Source Port.
     *
     * @param[in]  aPort  The UDP Source Port.
     *
     */
    void SetSourcePort(uint16_t aPort) { mSource = HostSwap16(aPort); }

    /**
     * This method returns the UDP Destination Port.
     *
     * @returns The UDP Destination Port.
     *
     */
    uint16_t GetDestinationPort(void) const { return HostSwap16(mDestination); }

    /**
     * This method sets the UDP Destination Port.
     *
     * @param[in]  aPort  The UDP Destination Port.
     *
     */
    void SetDestinationPort(uint16_t aPort) { mDestination = HostSwap16(aPort); }

    /**
     * This method returns the UDP Length.
     *
     * @returns The UDP Length.
     *
     */
    uint16_t GetLength(void) const { return HostSwap16(mLength); }

    /**
     * This method sets the UDP Length.
     *
     * @param[in]  aLength  The UDP Length.
     *
     */
    void SetLength(uint16_t aLength) { mLength = HostSwap16(aLength); }

    /**
     * This method returns the UDP Checksum.
     *
     * @returns The UDP Checksum.
     *
     */
    uint16_t GetChecksum(void) const { return HostSwap16(mChecksum); }

    /**
     * This method sets the UDP Checksum.
     *
     * @param[in]  aChecksum  The UDP Checksum.
     *
     */
    void SetChecksum(uint16_t aChecksum) { mChecksum = HostSwap16(aChecksum); }

    /**
     * This static method returns the byte offset for the UDP Length.
     *
     * @returns The byte offset for the UDP Length.
     *
     */
    static uint8_t GetLengthOffset(void) { return offsetof(UdpHeader, mLength); }

    /**
     * This static method returns the byte offset for the UDP Checksum.
     *
     * @returns The byte offset for the UDP Checksum.
     *
     */
    static uint8_t GetChecksumOffset(void) { return offsetof(UdpHeader, mChecksum); }

private:
    uint16_t mSource;
    uint16_t mDestination;
    uint16_t mLength;
    uint16_t mChecksum;
} __attribute__((packed));

/**
 * @}
 *
 */

}  // namespace Ip6
}  // namespace Thread

#endif  // NET_UDP6_HPP_
