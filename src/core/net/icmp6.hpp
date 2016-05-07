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
 *   This file includes definitions for ICMPv6.
 */

#ifndef ICMP6_HPP_
#define ICMP6_HPP_

#include <common/encoding.hpp>
#include <net/ip6.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Ip6 {

/**
 * @addtogroup core-ip6-icmp6
 *
 * @brief
 *   This module includes definitions for ICMPv6.
 *
 * @{
 *
 */

enum
{
    kIcmp6DataSize = 4,
};

/**
 * This structure represents an ICMPv6 header.
 *
 */
struct IcmpHeaderPoD
{
    uint8_t      mType;      ///< Type
    uint8_t      mCode;      ///< Code
    uint16_t     mChecksum;  ///< Checksum
    union
    {
        uint8_t  m8[kIcmp6DataSize / sizeof(uint8_t)];
        uint16_t m16[kIcmp6DataSize / sizeof(uint16_t)];
        uint32_t m32[kIcmp6DataSize / sizeof(uint32_t)];
    } mData;                 ///< Message-specific data
} __attribute__((packed));

/**
 * This class implements ICMPv6 header generation and parsing.
 *
 */
class IcmpHeader: private IcmpHeaderPoD
{
public:
    /**
     * This method initializes the ICMPv6 header to all zeros.
     *
     */
    void Init() { mType = 0; mCode = 0; mChecksum = 0; mData.m32[0] = 0; }

    /**
     * ICMPv6 Message Types
     *
     */
    enum Type
    {
        kTypeDstUnreach  = 0,     ///< Destination Unreachable
        kTypeEchoRequest = 128,   ///< Echo Request
        kTypeEchoReply   = 129,   ///< Echo Reply
    };

    /**
     * This method returns the ICMPv6 message type.
     *
     * @returns The ICMPv6 message type.
     *
     */
    Type GetType() const { return static_cast<Type>(mType); }

    /**
     * This method sets the ICMPv6 message type.
     *
     * @param[in]  aType  The ICMPv6 message type.
     *
     */
    void SetType(Type aType) { mType = static_cast<uint8_t>(aType); }

    /**
     * ICMPv6 Message Codes
     *
     */
    enum Code
    {
        kCodeDstUnreachNoRoute = 0,  ///< Destination Unreachable No Route
    };

    /**
     * This method returns the ICMPv6 message code.
     *
     * @returns The ICMPv6 message code.
     *
     */
    Code GetCode() const { return static_cast<Code>(mCode); }

    /**
     * This method sets the ICMPv6 message code.
     *
     * @param[in]  aCode  The ICMPv6 message code.
     */
    void SetCode(Code aCode) { mCode = static_cast<uint8_t>(aCode); }

    /**
     * This method returns the ICMPv6 message checksum.
     *
     * @returns The ICMPv6 message checksum.
     *
     */
    uint16_t GetChecksum() const { return HostSwap16(mChecksum); }

    /**
     * This method sets the ICMPv6 message checksum.
     *
     * @param[in]  aChecksum  The ICMPv6 message checksum.
     *
     */
    void SetChecksum(uint16_t aChecksum) { mChecksum = HostSwap16(aChecksum); }

    /**
     * This method returns the ICMPv6 message ID for Echo Requests and Replies.
     *
     * @returns The ICMPv6 message ID.
     *
     */
    uint16_t GetId() const { return HostSwap16(mData.m16[0]); }

    /**
     * This method sets the ICMPv6 message ID for Echo Requests and Replies.
     *
     * @param[in]  aId  The ICMPv6 message ID.
     *
     */
    void SetId(uint16_t aId) { mData.m16[0] = HostSwap16(aId); }

    /**
     * This method returns the ICMPv6 message sequence for Echo Requests and Replies.
     *
     * @returns The ICMPv6 message sequence.
     *
     */
    uint16_t GetSequence() const { return HostSwap16(mData.m16[1]); }

    /**
     * This method sets the ICMPv6 message sequence for Echo Requests and Replies.
     *
     * @param[in]  aSequence  The ICMPv6 message sequence.
     *
     */
    void SetSequence(uint16_t aSequence) { mData.m16[1] = HostSwap16(aSequence); }

    /**
     * This static method returns the byte offset of the Checksum field in the ICMPv6 header.
     *
     * @returns The byte offset of the Checksum field.
     *
     */
    static uint8_t GetChecksumOffset() { return offsetof(IcmpHeaderPoD, mChecksum); }

    /**
     * This static method returns the byte offset of the ICMPv6 payload.
     *
     * @returns The Byte offset of the ICMPv6 payload.
     *
     */
    static uint8_t GetDataOffset() { return offsetof(IcmpHeaderPoD, mData); }

} __attribute__((packed));

/**
 * This class implements an ICMPv6 echo client.
 *
 */
class IcmpEcho
{
    friend class Icmp;

public:
    /**
     * This function pointer is called when receiving an ICMPv6 Echo Reply in response to an Echo Request.
     *
     * @param[in]  aContext      A pointer to arbitrary context information.
     * @param[in]  aMessage      A reference to the received message.
     * @param[in]  aMessageInfo  A reference to message information associated with @p aMessage.
     *
     */
    typedef void (*EchoReplyHandler)(void *aContext, Message &aMessage, const MessageInfo &aMessageInfo);

    /**
     * This constructor creates an ICMPv6 echo client.
     *
     * @param[in]  aHandler  A pointer to a function that is called when receiving an ICMPv6 Echo Reply.
     * @param[in]  aContext  A pointer to arbitrary context information.
     *
     */
    IcmpEcho(EchoReplyHandler aHandler, void *aContext);

    /**
     * This method sends an ICMPv6 Echo Request message.
     *
     * @param[in]  aDestination    The socket address of the destination.
     * @param[in]  aPayload        A pointer to the data payload to send.
     * @param[in]  aPayloadLength  The number of data payoad bytes.
     *
     * @retval kThreadError_None    An ICMPv6 Echo Request message was enqueued.
     * @retval kThreadError_NoBufs  Insufficient buffers available to generate an ICMPv6 Echo Request message.
     *
     */
    ThreadError SendEchoRequest(const SockAddr &aDestination, const void *aPayload, uint16_t aPayloadLength);

private:
    void HandleEchoReply(Message &message, const MessageInfo &messageInfo) {
        mHandler(mContext, message, messageInfo);
    }

    EchoReplyHandler  mHandler;
    void             *mContext;
    uint16_t          mId;
    uint16_t          mSeq;
    IcmpEcho        *mNext;

    static uint16_t   sNextId;
    static IcmpEcho *sEchoClients;
};

/**
 * This class implements ICMPv6 message handlers.
 *
 */
class IcmpHandler
{
    friend class Icmp;

public:
    /**
     * This function pointer is called when receiving an ICMPv6 Destination Unreachable message.
     *
     * @param[in]  aContext      A pointer to arbitrary context information.
     * @param[in]  aMessage      A reference to the received message.
     * @param[in]  aMessageInfo  A reference to message information associated with @p aMessage.
     * @param[in]  aIcmpHeader   A reference to the receievd ICMPv6 header.
     *
     */
    typedef void (*DstUnreachHandler)(void *aContext, Message &aMesage, const MessageInfo &aMessageInfo,
                                      const IcmpHeader &aIcmpHeader);

    /**
     * This constructor creates an ICMPv6 message handler.
     *
     * @param[in]  aDstUnreachHandler  A pointer to the function that is called when receiving a Destination
     *                                 Unreachable message.
     * @param[in]  aContext            A pointer to arbitrary context information.
     *
     */
    IcmpHandler(DstUnreachHandler aDstUnreachHandler, void *aContext) {
        mDstUnreachHandler = aDstUnreachHandler;
        mContext = aContext;
    }

private:
    void HandleDstUnreach(Message &message, const MessageInfo &messageInfo, const IcmpHeader &icmp6Header) {
        mDstUnreachHandler(mContext, message, messageInfo, icmp6Header);
    }

    DstUnreachHandler    mDstUnreachHandler;
    void                *mContext;
    IcmpHandler        *mNext;

    static IcmpHandler *sHandlers;
};

/**
 * This class implements ICMPv6.
 *
 */
class Icmp
{
public:
    /**
     * This static method registers ICMPv6 handlers.
     *
     * @param[in]  aHandler  A reference to the ICMPv6 handler.
     *
     * @retval kThreadError_None  Successfully registered the ICMPv6 handler.
     * @retval kThreadError_Busy  The ICMPv6 handler is already registered.
     *
     */
    static ThreadError RegisterCallbacks(IcmpHandler &aHandler);

    /**
     * This static method sends an ICMPv6 error message.
     *
     * @param[in]  aDestination  The IPv6 destination address.
     * @param[in]  aType         The ICMPv6 message type.
     * @param[in]  aCode         The ICMPv6 message code.
     * @param[in]  aHeader       The IPv6 header of the error-causing message.
     *
     * @retval kThreadError_None    Successfully enqueued the ICMPv6 error message.
     * @retval kThreadError_NoBufs  Insufficient buffers available.
     *
     */
    static ThreadError SendError(const Address &aDestination, IcmpHeader::Type aType, IcmpHeader::Code aCode,
                                 const Header &aHeader);

    /**
     * This static method handles an ICMPv6 message.
     *
     * @param[in]  aMessage      A reference to the ICMPv6 mesasge.
     * @param[in]  aMessageInfo  A reference to the message info associated with @p aMessage.
     *
     * @retval kThreadError_None    Successfully processed the ICMPv6 message.
     * @retval kThreadError_NoBufs  Insufficient buffers available to generate the reply.
     * @retval kThreadError_Drop    The ICMPv6 message was invalid and dropped.
     *
     */
    static ThreadError HandleMessage(Message &aMessage, MessageInfo &aMessageInfo);

    /**
     * This static method updates the ICMPv6 checksum.
     *
     * @param[in]  aMessage               A reference to the ICMPv6 message.
     * @param[in]  aPseudoHeaderChecksum  The pseudo-header checksum value.
     *
     * @retval kThreadError_None         Successfully updated the ICMPv6 checksum.
     * @retval kThreadError_InvalidArgs  The message was invalid.
     *
     */
    static ThreadError UpdateChecksum(Message &aMessage, uint16_t aPseudoHeaderChecksum);

private:
    static ThreadError HandleDstUnreach(Message &aMessage, const MessageInfo &aMessageInfo,
                                        const IcmpHeader &aIcmpHeader);
    static ThreadError HandleEchoRequest(Message &aMessage, const MessageInfo &aMessageInfo);
    static ThreadError HandleEchoReply(Message &aMessage, const MessageInfo &aMessageInfo,
                                       const IcmpHeader &aIcmpHeader);
};

/**
 * @}
 *
 */

}  // namespace Ip6
}  // namespace Thread

#endif  // NET_ICMP6_HPP_
