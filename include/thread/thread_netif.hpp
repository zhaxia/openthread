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
 *   This file includes definitions for the Thread network interface.
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

/**
 * @addtogroup core-netif
 *
 * @brief
 *   This module includes definitions for the Thread network interface.
 *
 * @{
 */

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
    ThreadError RouteLookup(const Ip6Address &source, const Ip6Address &destination, uint8_t *prefixMatch) final;

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
    Coap::Server mCoapServer;
    AddressResolver mAddressResolver;
    KeyManager mKeyManager;
    Lowpan mLowpan;
    Mac::Mac mMac;
    MeshForwarder mMeshForwarder;
    Mle::MleRouter mMleRouter;
    NetworkData::Local mNetworkDataLocal;
    NetworkData::Leader mNetworkDataLeader;
    bool mIsUp;
};

struct ThreadMessageInfo
{
    uint8_t mLinkMargin;
};

/**
 * @}
 */

}  // namespace Thread

#endif  // THREAD_NETIF_HPP_
