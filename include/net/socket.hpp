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

#ifndef NET_SOCKET_HPP_
#define NET_SOCKET_HPP_

#include <net/ip6_address.hpp>

namespace Thread {

typedef struct
{
    Ip6Address mSockAddr;
    Ip6Address mPeerAddr;
    uint16_t mPeerPort;
    uint16_t mSockPort;
    uint8_t mInterfaceId;
    uint8_t mHopLimit;
    const void *mLinkInfo;
} Ip6MessageInfo;

class Socket
{
protected:
    struct sockaddr_in6 mSockName;
    struct sockaddr_in6 mPeerName;
};

}  // namespace Thread

#endif  // NET_SOCKET_HPP_
