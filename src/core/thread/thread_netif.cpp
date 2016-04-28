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
 *   This file implements the Thread network interface.
 */

#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <net/ip6.hpp>
#include <net/netif.hpp>
#include <net/udp6.hpp>
#include <thread/mle.hpp>
#include <thread/thread_netif.hpp>
#include <thread/thread_tlvs.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

static const uint8_t kThreadMasterKey[] =
{
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

static const char name[] = "thread";

ThreadNetif::ThreadNetif():
    mCoapServer(kCoapUdpPort),
    mAddressResolver(*this),
    mKeyManager(*this),
    mLowpan(*this),
    mMeshForwarder(*this),
    mMleRouter(*this),
    mNetworkDataLocal(*this),
    mNetworkDataLeader(*this)
{
}

const char *ThreadNetif::GetName() const
{
    return name;
}

ThreadError ThreadNetif::Init()
{
    mKeyManager.SetMasterKey(kThreadMasterKey, sizeof(kThreadMasterKey));
    mMac.Init(*this);
    mMleRouter.Init();
    return kThreadError_None;
}

ThreadError ThreadNetif::Up()
{
    Netif::AddNetif();
    mMeshForwarder.Start();
    mMleRouter.Start();
    mCoapServer.Start();
    mIsUp = true;
    return kThreadError_None;
}

ThreadError ThreadNetif::Down()
{
    mCoapServer.Stop();
    mMleRouter.Stop();
    mMeshForwarder.Stop();
    Netif::RemoveNetif();
    mIsUp = false;
    return kThreadError_None;
}

bool ThreadNetif::IsUp() const
{
    return mIsUp;
}

ThreadError ThreadNetif::GetLinkAddress(Ip6::LinkAddress &address) const
{
    address.mType = Ip6::LinkAddress::kEui64;
    address.mLength = 8;
    memcpy(&address.mExtAddress, mMac.GetExtAddress(), address.mLength);
    return kThreadError_None;
}

ThreadError ThreadNetif::RouteLookup(const Ip6::Address &source, const Ip6::Address &destination, uint8_t *prefixMatch)
{
    return mNetworkDataLeader.RouteLookup(source, destination, prefixMatch, NULL);
}

AddressResolver *ThreadNetif::GetAddressResolver()
{
    return &mAddressResolver;
}

Coap::Server *ThreadNetif::GetCoapServer()
{
    return &mCoapServer;
}

KeyManager *ThreadNetif::GetKeyManager()
{
    return &mKeyManager;
}

Lowpan *ThreadNetif::GetLowpan()
{
    return &mLowpan;
}

Mac::Mac *ThreadNetif::GetMac()
{
    return &mMac;
}

Mle::MleRouter *ThreadNetif::GetMle()
{
    return &mMleRouter;
}

MeshForwarder *ThreadNetif::GetMeshForwarder()
{
    return &mMeshForwarder;
}

NetworkData::Local *ThreadNetif::GetNetworkDataLocal()
{
    return &mNetworkDataLocal;
}

NetworkData::Leader *ThreadNetif::GetNetworkDataLeader()
{
    return &mNetworkDataLeader;
}

ThreadError ThreadNetif::SendMessage(Message &message)
{
    return mMeshForwarder.SendMessage(message);
}

}  // namespace Thread
