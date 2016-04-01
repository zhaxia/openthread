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

#ifndef ADDRESS_RESOLVER_H_
#define ADDRESS_RESOLVER_H_

#include <coap/coap_server.h>
#include <common/thread_error.h>
#include <common/timer.h>
#include <mac/mac.h>
#include <net/icmp6.h>
#include <net/udp6.h>

namespace Thread {

class MeshForwarder;
class ThreadLastTransactionTimeTlv;
class ThreadMeshLocalIidTlv;
class ThreadNetif;
class ThreadTargetTlv;

class AddressResolver
{
public:
    class Cache
    {
    public:
        Ip6Address target;
        uint8_t iid[8];
        Mac::Address16 rloc;
        uint8_t timeout;
        uint8_t failure_count : 4;
        enum State
        {
            kStateInvalid = 0,
            kStateDiscover = 1,
            kStateRetry = 2,
            kStateValid = 3,
        };
        State state : 2;
    };

    explicit AddressResolver(ThreadNetif &netif);
    ThreadError Clear();
    ThreadError Remove(uint8_t router_id);
    ThreadError Resolve(const Ip6Address &eid, Mac::Address16 &rloc);

    const Cache *GetCacheEntries(uint16_t *num_entries) const;

private:
    enum
    {
        kCacheEntries = 8,
        kDiscoverTimeout = 3,  // seconds
    };

    ThreadError SendAddressQuery(const Ip6Address &eid);
    ThreadError SendAddressError(const ThreadTargetTlv &target, const ThreadMeshLocalIidTlv &eid,
                                 const Ip6Address *destination);
    void SendAddressQueryResponse(const ThreadTargetTlv &target_tlv, const ThreadMeshLocalIidTlv &ml_iid_tlv,
                                  const ThreadLastTransactionTimeTlv *last_transaction_time_tlv,
                                  const Ip6Address &destination);
    void SendAddressNotificationResponse(const Coap::Header &request_header, const Ip6MessageInfo &message_info);

    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);

    static void HandleAddressError(void *context, Coap::Header &header,
                                   Message &message, const Ip6MessageInfo &message_info);
    void HandleAddressError(Coap::Header &header, Message &message, const Ip6MessageInfo &message_info);

    static void HandleAddressQuery(void *context, Coap::Header &header,
                                   Message &message, const Ip6MessageInfo &message_info);
    void HandleAddressQuery(Coap::Header &header, Message &message, const Ip6MessageInfo &message_info);

    static void HandleAddressNotification(void *context, Coap::Header &header,
                                          Message &message, const Ip6MessageInfo &message_info);
    void HandleAddressNotification(Coap::Header &header, Message &message, const Ip6MessageInfo &message_info);

    static void HandleDstUnreach(void *context, Message &message, const Ip6MessageInfo &message_info,
                                 const Icmp6Header &icmp6_header);
    void HandleDstUnreach(Message &message, const Ip6MessageInfo &message_info, const Icmp6Header &icmp6_header);

    static void HandleTimer(void *context);
    void HandleTimer();

    Coap::Resource m_address_error;
    Coap::Resource m_address_query;
    Coap::Resource m_address_notification;
    Cache m_cache[kCacheEntries];
    uint16_t m_coap_message_id;
    uint8_t m_coap_token[2];
    Icmp6Handler m_icmp6_handler;
    Udp6Socket m_socket;
    Timer m_timer;

    MeshForwarder *m_mesh_forwarder;
    Coap::Server *m_coap_server;
    Mle::MleRouter *m_mle;
    Netif *m_netif;
};

}  // namespace Thread

#endif  // ADDRESS_RESOLVER_H_
