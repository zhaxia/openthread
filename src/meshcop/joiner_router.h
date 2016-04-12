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

#ifndef MESHCOP_JOINER_ROUTER_H_
#define MESHCOP_JOINER_ROUTER_H_

#include <coap/coap_server.h>
#include <common/thread_error.h>
#include <net/udp6.h>
#include <mac/mac.h>

namespace Thread {

class ThreadNetif;

namespace Meshcop {

class JoinerRouter
{
    Netif *m_netif;
    Udp6Socket m_socket;
    Timer m_timer;

    uint16_t m_coap_message_id;
    uint8_t m_coap_token[2];

    Coap::Server *m_coap_server;
    Coap::Resource m_coap_joiner_entrust;
    Coap::Resource m_coap_relay_tx;

public:
    explicit JoinerRouter(ThreadNetif &netif);

    ThreadError SendRelayRx(Message &joiner_frame, 
			    const Ip6MessageInfo &message_info);

    ThreadError SendRelayTxDecapsulated(const Coap::Header &request_header, 
					const Ip6MessageInfo &message_info,
					const uint8_t *tlvs, 
					uint8_t tlvs_length);

    static void HandleTimer(void *context);
    void HandleTimer();

    static void HandleUdpReceive(void *context, Message &message, 
				 const Ip6MessageInfo &message_info);

    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);

    static void HandleRelayTx(void *context, Coap::Header &header,
			      Message &message, 
			      const Ip6MessageInfo &message_info);

    void HandleRelayTx(Coap::Header &header, Message &message, 
		       const Ip6MessageInfo &message_info);

    static void HandleJoinerEntrust(void *context, Coap::Header &header,
				    Message &message, 
				    const Ip6MessageInfo &message_info);

    void HandleJoinerEntrust(Coap::Header &header, Message &message, 
			     const Ip6MessageInfo &message_info);

};

}  // namespace Meshcop
}  // namespace Thread

#endif  // MESHCOP_JOINER_ROUTER_H_
