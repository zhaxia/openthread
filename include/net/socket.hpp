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
 *   This file includes definitions for IPv6 sockets.
 */

#ifndef NET_SOCKET_HPP_
#define NET_SOCKET_HPP_

#include <openthread.h>
#include <net/ip6_address.hpp>

namespace Thread {
namespace Ip6 {

/**
 * @addtogroup core-ip6-ip6
 *
 * @{
 *
 */

class MessageInfo: public otMessageInfo
{
public:
    Address &GetSockAddr() { return *static_cast<Address *>(&mSockAddr);}
    const Address &GetSockAddr() const { return *static_cast<const Address *>(&mSockAddr);}
    Address &GetPeerAddr() { return *static_cast<Address *>(&mPeerAddr);}
    const Address &GetPeerAddr() const { return *static_cast<const Address *>(&mPeerAddr);}
};

class SockAddr: public otSockAddr
{
public:
    Address &GetAddress() { return *static_cast<Address *>(&mAddress); }
    const Address &GetAddress() const { return *static_cast<const Address *>(&mAddress); }
};

/**
 * @}
 */

}  // namespace Ip6
}  // namespace Thread

#endif  // NET_SOCKET_HPP_
