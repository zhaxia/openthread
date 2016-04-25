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

class Resource
{
    friend class Server;

public:
    typedef void (*CoapMessageHandler)(void *context, Header &header, Message &message,
                                       const Ip6MessageInfo &messageInfo);
    Resource(const char *uriPath, CoapMessageHandler handler, void *context) {
        mUriPath = uriPath;
        mHandler = handler;
        mContext = context;
    }

private:
    const char *mUriPath;
    CoapMessageHandler mHandler;
    void *mContext;
    Resource *mNext;
};

class Server
{
public:
    explicit Server(uint16_t port);
    ThreadError Start();
    ThreadError Stop();
    ThreadError AddResource(Resource &resource);
    ThreadError SendMessage(Message &message, const Ip6MessageInfo &messageInfo);

private:
    static void HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &messageInfo);

    Udp6Socket mSocket;
    uint16_t mPort;
    Resource *mResources = NULL;
};

/**
 * @}
 *
 */

}  // namespace Coap
}  // namespace Thread

#endif  // COAP_SERVER_HPP_
