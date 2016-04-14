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

#ifndef NETWORK_DATA_LEADER_HPP_
#define NETWORK_DATA_LEADER_HPP_

#include <stdint.h>

#include <coap/coap_server.hpp>
#include <common/timer.hpp>
#include <net/ip6_address.hpp>
#include <thread/mle_router.hpp>
#include <thread/network_data.hpp>

namespace Thread {

class ThreadNetif;

namespace NetworkData {

class Leader: public NetworkData
{
public:
    explicit Leader(ThreadNetif &netif);
    ThreadError Init();
    ThreadError Start();
    ThreadError Stop();

    uint8_t GetVersion() const;
    uint8_t GetStableVersion() const;

    uint32_t GetContextIdReuseDelay() const;
    ThreadError SetContextIdReuseDelay(uint32_t delay);

    ThreadError GetContext(const Ip6Address &address, Context &context);
    ThreadError GetContext(uint8_t context_id, Context &context);

    bool IsOnMesh(const Ip6Address &address);
    ThreadError RouteLookup(const Ip6Address &source, const Ip6Address &destination,
                            uint8_t *prefix_match, uint16_t *rloc);
    ThreadError SetNetworkData(uint8_t version, uint8_t stable_version, bool stable_only,
                               const uint8_t *data, uint8_t data_length);
    ThreadError RemoveBorderRouter(uint16_t rloc);

private:
    static void HandleServerData(void *context, Coap::Header &header, Message &message,
                                 const Ip6MessageInfo &message_info);
    void HandleServerData(Coap::Header &header, Message &message, const Ip6MessageInfo &message_info);
    void SendServerDataResponse(const Coap::Header &request_header, const Ip6MessageInfo &message_info,
                                const uint8_t *tlvs, uint8_t tlvs_length);

    static void HandleTimer(void *context);
    void HandleTimer();

    ThreadError RegisterNetworkData(uint16_t rloc, uint8_t *tlvs, uint8_t tlvs_length);

    ThreadError AddHasRoute(PrefixTlv &prefix, HasRouteTlv &src);
    ThreadError AddBorderRouter(PrefixTlv &prefix, BorderRouterTlv &src);
    ThreadError AddNetworkData(uint8_t *tlv, uint8_t tlv_length);
    ThreadError AddPrefix(PrefixTlv &tlv);

    int AllocateContext();
    ThreadError FreeContext(uint8_t context_id);

    ThreadError ConfigureAddresses();
    ThreadError ConfigureAddress(PrefixTlv &prefix);

    ContextTlv *FindContext(PrefixTlv &prefix);

    ThreadError RemoveContext(uint8_t context_id);
    ThreadError RemoveContext(PrefixTlv &prefix, uint8_t context_id);

    ThreadError RemoveRloc(uint16_t rloc);
    ThreadError RemoveRloc(PrefixTlv &prefix, uint16_t rloc);
    ThreadError RemoveRloc(PrefixTlv &prefix, HasRouteTlv &has_route, uint16_t rloc);
    ThreadError RemoveRloc(PrefixTlv &prefix, BorderRouterTlv &border_router, uint16_t rloc);

    ThreadError ExternalRouteLookup(uint8_t domain_id, const Ip6Address &destination,
                                    uint8_t *prefix_match, uint16_t *rloc);
    ThreadError DefaultRouteLookup(PrefixTlv &prefix, uint16_t *rloc);

    Coap::Resource m_server_data;
    uint8_t stable_version_;
    uint8_t version_;

    NetifUnicastAddress addresses_[4];

    enum
    {
        kMinContextId = 1,
        kNumContextIds = 15,
        kContextIdReuseDelay = 48 * 60 * 60,
    };
    uint16_t context_used_ = 0;
    uint32_t m_context_last_used[kNumContextIds];
    uint32_t m_context_id_reuse_delay = kContextIdReuseDelay;
    Timer m_timer;

    Coap::Server *m_coap_server;
    Netif *m_netif;
    Mle::MleRouter *m_mle;
};

}  // namespace NetworkData
}  // namespace Thread

#endif  // NETWORK_DATA_LEADER_HPP_
