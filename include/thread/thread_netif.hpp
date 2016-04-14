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

#ifndef THREAD_NETIF_HPP_
#define THREAD_NETIF_HPP_

#include <common/thread_error.hpp>
#include <mac/mac.hpp>
#include <net/netif.hpp>
#include <thread/address_resolver.hpp>
#include <thread/key_manager.hpp>
#include <thread/mesh_forwarder.hpp>
#include <thread/mle.hpp>
#include <thread/mle_router.hpp>
#include <thread/network_data_local.hpp>

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

#endif  // THREAD_NETIF_HPP_
