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

#ifndef NET_SOCKET_H_
#define NET_SOCKET_H_

#include <net/ip6_address.h>

namespace Thread {

typedef struct
{
    Ip6Address sock_addr;
    Ip6Address peer_addr;
    uint16_t peer_port;
    uint16_t sock_port;
    uint8_t interface_id;
    uint8_t hop_limit;
    const void *link_info;
} Ip6MessageInfo;

class Socket
{
protected:
    struct sockaddr_in6 m_sockname;
    struct sockaddr_in6 m_peername;
};

}  // namespace Thread

#endif  // NET_SOCKET_H_
