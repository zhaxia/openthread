/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
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
