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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
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
