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

#ifndef NETWORK_DATA_LOCAL_HPP_
#define NETWORK_DATA_LOCAL_HPP_

#include <net/udp6.hpp>
#include <thread/mle_router.hpp>
#include <thread/network_data.hpp>

namespace Thread {

class ThreadNetif;

namespace NetworkData {

class Local: public NetworkData
{
public:
    explicit Local(ThreadNetif &netif);
    ThreadError AddOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length, int8_t prf, uint8_t flags, bool stable);
    ThreadError RemoveOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length);

    ThreadError AddHasRoutePrefix(const uint8_t *prefix, uint8_t prefix_length, int8_t prf, bool stable);
    ThreadError RemoveHasRoutePrefix(const uint8_t *prefix, uint8_t prefix_length);

    ThreadError Register(const Ip6Address &destination);

private:
    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);

    ThreadError UpdateRloc();
    ThreadError UpdateRloc(PrefixTlv &prefix);
    ThreadError UpdateRloc(HasRouteTlv &has_route);
    ThreadError UpdateRloc(BorderRouterTlv &border_router);

    Udp6Socket m_socket;
    uint8_t m_coap_token[2];
    uint16_t m_coap_message_id;

    Mle::MleRouter *m_mle;
};

}  // namespace NetworkData
}  // namespace Thread

#endif  // NETWORK_DATA_LOCAL_HPP_
