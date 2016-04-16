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
    ThreadError GetContext(uint8_t contextId, Context &context);

    bool IsOnMesh(const Ip6Address &address);
    ThreadError RouteLookup(const Ip6Address &source, const Ip6Address &destination,
                            uint8_t *prefixMatch, uint16_t *rloc);
    ThreadError SetNetworkData(uint8_t version, uint8_t stableVersion, bool stable_only,
                               const uint8_t *data, uint8_t dataLength);
    ThreadError RemoveBorderRouter(uint16_t rloc);

private:
    static void HandleServerData(void *context, Coap::Header &header, Message &message,
                                 const Ip6MessageInfo &messageInfo);
    void HandleServerData(Coap::Header &header, Message &message, const Ip6MessageInfo &messageInfo);
    void SendServerDataResponse(const Coap::Header &requestHeader, const Ip6MessageInfo &messageInfo,
                                const uint8_t *tlvs, uint8_t tlvsLength);

    static void HandleTimer(void *context);
    void HandleTimer();

    ThreadError RegisterNetworkData(uint16_t rloc, uint8_t *tlvs, uint8_t tlvsLength);

    ThreadError AddHasRoute(PrefixTlv &prefix, HasRouteTlv &src);
    ThreadError AddBorderRouter(PrefixTlv &prefix, BorderRouterTlv &src);
    ThreadError AddNetworkData(uint8_t *tlv, uint8_t tlvLength);
    ThreadError AddPrefix(PrefixTlv &tlv);

    int AllocateContext();
    ThreadError FreeContext(uint8_t contextId);

    ThreadError ConfigureAddresses();
    ThreadError ConfigureAddress(PrefixTlv &prefix);

    ThreadError RemoveContext(uint8_t contextId);
    ThreadError RemoveContext(PrefixTlv &prefix, uint8_t contextId);

    ThreadError RemoveRloc(uint16_t rloc);
    ThreadError RemoveRloc(PrefixTlv &prefix, uint16_t rloc);
    ThreadError RemoveRloc(PrefixTlv &prefix, HasRouteTlv &hasRoute, uint16_t rloc);
    ThreadError RemoveRloc(PrefixTlv &prefix, BorderRouterTlv &borderRouter, uint16_t rloc);

    ThreadError ExternalRouteLookup(uint8_t domainId, const Ip6Address &destination,
                                    uint8_t *prefixMatch, uint16_t *rloc);
    ThreadError DefaultRouteLookup(PrefixTlv &prefix, uint16_t *rloc);

    Coap::Resource mServerData;
    uint8_t mStableVersion;
    uint8_t mVersion;

    NetifUnicastAddress mAddresses[4];

    enum
    {
        kMinContextId = 1,
        kNumContextIds = 15,
        kContextIdReuseDelay = 48 * 60 * 60,
    };
    uint16_t mContextUsed = 0;
    uint32_t mContextLastUsed[kNumContextIds];
    uint32_t mContextIdReuseDelay = kContextIdReuseDelay;
    Timer mTimer;

    Coap::Server *mCoapServer;
    Netif *mNetif;
    Mle::MleRouter *mMle;
};

}  // namespace NetworkData
}  // namespace Thread

#endif  // NETWORK_DATA_LEADER_HPP_
