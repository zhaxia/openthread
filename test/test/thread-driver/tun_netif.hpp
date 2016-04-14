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

#ifndef TUN_NETIF_HPP_
#define TUN_NETIF_HPP_

#include <netinet/in.h>

#include <ncp/ncp.pb-c.h>

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>

namespace Thread {

class TunNetif
{
public:
    class Callbacks
    {
        void HandleReceive(uint8_t *buf, size_t buf_length);
    };

    ThreadError Open();
    ThreadError GetName(char *name, int name_length);
    int GetIndex();

    ThreadError Down();
    ThreadError Up();

    int GetFileDescriptor();
    size_t Read(uint8_t *buf, size_t buf_length);
    size_t Write(uint8_t *buf, size_t buf_length);

    ThreadError AddIp6Address(const struct in6_addr *address, uint8_t prefix_length);
    ThreadError RemoveIp6Address(const struct in6_addr *address);
    ThreadError SetIp6Addresses(ThreadIp6Addresses *addresses);

    ThreadError AddRoute(const struct in6_addr *prefix, uint8_t prefix_length);
    ThreadError ClearRoutes();

private:
    int tunfd_;
};

}  // namespace Thread

#endif  // TUN_NETIF_HPP_
