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

#ifndef THREAD_NETIF_H_
#define THREAD_NETIF_H_

#include <common/thread_error.h>
#include <mac/mac.h>
#include <net/netif.h>
#include <thread/address_resolver.h>
#include <thread/key_manager.h>
#include <thread/mesh_forwarder.h>
#include <thread/mle.h>
#include <thread/mle_router.h>
#include <thread/network_data_local.h>

namespace Thread {

class ThreadNetif: public Netif
{
public:
    ThreadNetif();
    ThreadError Init();
    ThreadError Up();
    ThreadError Down();
    bool IsUp() const;

    const char *GetName() const final;
    ThreadError GetLinkAddress(LinkAddress &address) const final;
    ThreadError SendMessage(Message &message) final;
    ThreadError RouteLookup(const Ip6Address &source, const Ip6Address &destination, uint8_t *prefix_match) final;

    AddressResolver *GetAddressResolver();
    Coap::Server *GetCoapServer();
    KeyManager *GetKeyManager();
    Lowpan *GetLowpan();
    Mac::Mac *GetMac();
    Mle::MleRouter *GetMle();
    MeshForwarder *GetMeshForwarder();
    NetworkData::Local *GetNetworkDataLocal();
    NetworkData::Leader *GetNetworkDataLeader();

private:
    Coap::Server m_coap_server;
    AddressResolver m_address_resolver;
    KeyManager m_key_manager;
    Lowpan m_lowpan;
    Mac::Mac m_mac;
    MeshForwarder m_mesh_forwarder;
    Mle::MleRouter m_mle_router;
    NetworkData::Local m_network_data_local;
    NetworkData::Leader m_network_data_leader;
    bool m_is_up;
};

struct ThreadMessageInfo
{
    uint8_t link_margin;
};

}  // namespace Thread

#endif  // THREAD_NETIF_H_
