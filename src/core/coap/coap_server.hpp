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
 *   This file includes definitions for the CoAP server.
 */

#ifndef COAP_SERVER_HPP_
#define COAP_SERVER_HPP_

#include <coap/coap_header.hpp>
#include <common/message.hpp>
#include <net/udp6.hpp>

namespace Thread {
namespace Coap {

/**
 * @addtogroup core-coap
 *
 * @{
 *
 */

/**
 * This class implements CoAP resource handling.
 *
 */
class Resource
{
    friend class Server;

public:
    /**
     * This function pointer is called when a CoAP message with a given Uri-Path is received.
     *
     * @param[in]  aContext      A pointer to arbitrary context information.
     * @param[in]  aHeader       A reference to the CoAP header.
     * @param[in]  aMessage      A reference to the message.
     * @param[in]  aMessageInfo  A reference to the message info for @p aMessage.
     *
     */
    typedef void (*CoapMessageHandler)(void *aContext, Header &aHeader, Message &aMessage,
                                       const Ip6::MessageInfo &aMessageInfo);

    /**
     * This constructor initializes the resource.
     *
     * @param[in]  aUriPath  A pointer to a NULL-terminated string for the Uri-Path.
     * @param[in]  aHandler  A function pointer that is called when receiving a CoAP message for @p aUriPath.
     * @param[in]  aContext  A pointer to arbitrary context information.
     */
    Resource(const char *aUriPath, CoapMessageHandler aHandler, void *aContext) {
        mUriPath = aUriPath;
        mHandler = aHandler;
        mContext = aContext;
    }

private:
    void HandleRequest(Header &aHeader, Message &aMessage, const Ip6::MessageInfo &aMessageInfo) {
        mHandler(mContext, aHeader, aMessage, aMessageInfo);
    }

    const char *mUriPath;
    CoapMessageHandler mHandler;
    void *mContext;
    Resource *mNext;
};

/**
 * This class implements the CoAP server.
 *
 */
class Server
{
public:
    /**
     * This constructor initializes the object.
     *
     */
    explicit Server(uint16_t aPort);

    /**
     * This method starts the CoAP server.
     *
     * @retval kThreadError_None  Successfully started the CoAP server.
     *
     */
    ThreadError Start();

    /**
     * This method stops the CoAP server.
     *
     * @retval kThreadError_None  Successfully stopped the CoAP server.
     *
     */
    ThreadError Stop();

    /**
     * This method adds a resource to the CoAP server.
     *
     * @param[in]  aResource  A reference to the resource.
     *
     * @retval kThreadError_None  Successfully added @p aResource.
     * @retval kThreadError_Busy  The @p aResource was alerady added.
     *
     */
    ThreadError AddResource(Resource &aResource);

    /**
     * This method sends a CoAP response from the server.
     *
     * @param[in]  aMessage      The CoAP response to send.
     * @param[in]  aMessageInfo  The message info corresponding to @p aMessage.
     *
     * @retval kThreadError_None    Successfully enqueued the CoAP response message.
     * @retval kThreadError_NoBufs  Insufficient buffers available to send the CoAP response.
     *
     */
    ThreadError SendMessage(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);

private:
    enum
    {
        kMaxReceivedUriPath = 32,   ///< Maximum supported URI path on received messages.
    };

    static void HandleUdpReceive(void *aContext, otMessage aMessage, const otMessageInfo *aMessageInfo);
    void HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);

    Ip6::UdpSocket mSocket;
    uint16_t mPort;
    Resource *mResources;
};

/**
 * @}
 *
 */

}  // namespace Coap
}  // namespace Thread

#endif  // COAP_SERVER_HPP_
