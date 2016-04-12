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

#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/message.h>
#include <common/thread_error.h>
#include <net/ip6.h>
#include <net/netif.h>
#include <net/udp6.h>
#include <thread/mle.h>
#include <thread/thread_netif.h>
#include <thread/thread_tlvs.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static const uint8_t kThreadMasterKey[] =
{
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static const char name[] = "thread";

ThreadNetif::ThreadNetif():
    m_coap_server(kCoapUdpPort),
    m_address_resolver(*this),
    m_key_manager(*this),
    m_lowpan(*this),
    m_mac(this),
    m_mesh_forwarder(*this),
    m_mle_router(*this),
    m_network_data_local(*this),
    m_network_data_leader(*this)
{
}

const char *ThreadNetif::GetName() const
{
    return name;
}

ThreadError ThreadNetif::Init()
{
    m_key_manager.SetMasterKey(kThreadMasterKey, sizeof(kThreadMasterKey));
    m_mac.Init();
    m_mle_router.Init();
    return kThreadError_None;
}

ThreadError ThreadNetif::Up()
{
    Netif::AddNetif();
    m_mesh_forwarder.Start();
    m_mle_router.Start();
    m_coap_server.Start();
    m_is_up = true;
    return kThreadError_None;
}

ThreadError ThreadNetif::Down()
{
    m_coap_server.Stop();
    m_mle_router.Stop();
    m_mesh_forwarder.Stop();
    Netif::RemoveNetif();
    m_is_up = false;
    return kThreadError_None;
}

bool ThreadNetif::IsUp() const
{
    return m_is_up;
}

ThreadError ThreadNetif::GetLinkAddress(LinkAddress &address) const
{
    address.type = LinkAddress::kEui64;
    address.length = 8;
    memcpy(&address.address64, m_mac.GetAddress64(), address.length);
    return kThreadError_None;
}

ThreadError ThreadNetif::RouteLookup(const Ip6Address &source, const Ip6Address &destination, uint8_t *prefix_match)
{
    return m_network_data_leader.RouteLookup(source, destination, prefix_match, NULL);
}

AddressResolver *ThreadNetif::GetAddressResolver()
{
    return &m_address_resolver;
}

Coap::Server *ThreadNetif::GetCoapServer()
{
    return &m_coap_server;
}

KeyManager *ThreadNetif::GetKeyManager()
{
    return &m_key_manager;
}

Lowpan *ThreadNetif::GetLowpan()
{
    return &m_lowpan;
}

Mac::Mac *ThreadNetif::GetMac()
{
    return &m_mac;
}

Mle::MleRouter *ThreadNetif::GetMle()
{
    return &m_mle_router;
}

MeshForwarder *ThreadNetif::GetMeshForwarder()
{
    return &m_mesh_forwarder;
}

NetworkData::Local *ThreadNetif::GetNetworkDataLocal()
{
    return &m_network_data_local;
}

NetworkData::Leader *ThreadNetif::GetNetworkDataLeader()
{
    return &m_network_data_leader;
}

ThreadError ThreadNetif::SendMessage(Message &message)
{
    return m_mesh_forwarder.SendMessage(message);
}

}  // namespace Thread
