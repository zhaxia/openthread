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

#ifndef COAP_SERVER_H_
#define COAP_SERVER_H_

#include <coap/coap_header.h>
#include <common/message.h>
#include <net/udp6.h>

namespace Thread {
namespace Coap {

class Resource
{
    friend class Server;

public:
    typedef void (*CoapMessageHandler)(void *context, Header &header, Message &message,
                                       const Ip6MessageInfo &message_info);
    Resource(const char *uri_path, CoapMessageHandler handler, void *context) {
        m_uri_path = uri_path;
        m_handler = handler;
        m_context = context;
    }

private:
    const char *m_uri_path;
    CoapMessageHandler m_handler;
    void *m_context;
    Resource *m_next;
};

class Server
{
public:
    explicit Server(uint16_t port);
    ThreadError Start();
    ThreadError Stop();
    ThreadError AddResource(Resource &resource);
    ThreadError SendMessage(Message &message, const Ip6MessageInfo &message_info);

private:
    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);

    Udp6Socket m_socket;
    uint16_t m_port;
    Resource *m_resources = NULL;
};

}  // namespace Coap
}  // namespace Thread

#endif  // COAP_COAP_SERVER_H_
