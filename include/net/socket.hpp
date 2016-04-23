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
 *   This file contains definitions for IPv6 sockets.
 */

#ifndef NET_SOCKET_HPP_
#define NET_SOCKET_HPP_

#include <openthread.h>
#include <net/ip6_address.hpp>

namespace Thread {

class Ip6MessageInfo: public otMessageInfo
{
public:
    Ip6Address &GetSockAddr() { return *static_cast<Ip6Address *>(&mSockAddr);}
    const Ip6Address &GetSockAddr() const { return *static_cast<const Ip6Address *>(&mSockAddr);}
    Ip6Address &GetPeerAddr() { return *static_cast<Ip6Address *>(&mPeerAddr);}
    const Ip6Address &GetPeerAddr() const { return *static_cast<const Ip6Address *>(&mPeerAddr);}
};

class SockAddr: public otSockAddr
{
public:
    Ip6Address &GetAddress() { return *static_cast<Ip6Address *>(&mAddress); }
    const Ip6Address &GetAddress() const { return *static_cast<const Ip6Address *>(&mAddress); }
};

}  // namespace Thread

#endif  // NET_SOCKET_HPP_
