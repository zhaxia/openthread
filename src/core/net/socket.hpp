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

/**
 * This class implements message information for an IPv6 message.
 *
 */
class MessageInfo: public otMessageInfo
{
public:
    /**
     * This method returns a reference to the local socket address.
     *
     * @returns A reference to the local socket address.
     *
     */
    Address &GetSockAddr(void) { return *static_cast<Address *>(&mSockAddr);}

    /**
     * This method returns a reference to the local socket address.
     *
     * @returns A reference to the local socket address.
     *
     */
    const Address &GetSockAddr(void) const { return *static_cast<const Address *>(&mSockAddr);}

    /**
     * This method returns a reference to the peer socket address.
     *
     * @returns A reference to the peer socket address.
     *
     */
    Address &GetPeerAddr(void) { return *static_cast<Address *>(&mPeerAddr);}

    /**
     * This method returns a reference to the peer socket address.
     *
     * @returns A reference to the peer socket address.
     *
     */
    const Address &GetPeerAddr(void) const { return *static_cast<const Address *>(&mPeerAddr);}
};

/**
 * This class implements a socket address.
 *
 */
class SockAddr: public otSockAddr
{
public:
    /**
     * This constructor initializes the object.
     *
     */
    SockAddr(void) { memset(&mAddress, 0, sizeof(mAddress)), mPort = 0, mScopeId = 0; }

    /**
     * This method returns a reference to the IPv6 address.
     *
     * @returns A reference to the IPv6 address.
     *
     */
    Address &GetAddress(void) { return *static_cast<Address *>(&mAddress); }

    /**
     * This method returns a reference to the IPv6 address.
     *
     * @returns A reference to the IPv6 address.
     *
     */
    const Address &GetAddress(void) const { return *static_cast<const Address *>(&mAddress); }
};

/**
 * @}
 */

}  // namespace Ip6
}  // namespace Thread

#endif  // NET_SOCKET_HPP_
