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
 *   This file includes definitions for Thread EID-to-RLOC mapping and caching.
 */

#ifndef ADDRESS_RESOLVER_HPP_
#define ADDRESS_RESOLVER_HPP_

#include <openthread-core-config.h>
#include <openthread-types.h>
#include <coap/coap_server.hpp>
#include <common/timer.hpp>
#include <mac/mac.hpp>
#include <net/icmp6.hpp>
#include <net/udp6.hpp>

namespace Thread {

class MeshForwarder;
class ThreadLastTransactionTimeTlv;
class ThreadMeshLocalEidTlv;
class ThreadNetif;
class ThreadTargetTlv;

/**
 * @addtogroup core-arp
 *
 * @brief
 *   This module includes definitions for Thread EID-to-RLOC mapping and caching.
 *
 * @{
 */

/**
 * This class implements the EID-to-RLOC mapping and caching.
 *
 */
class AddressResolver
{
public:
    explicit AddressResolver(ThreadNetif &netif);

    /**
     * This method clears the EID-to-RLOC cache.
     *
     */
    void Clear(void);

    /**
     * This method removes a Router ID from the EID-to-RLOC cache.
     *
     * @param[in]  aRouterId  The Router ID.
     *
     */
    void Remove(uint8_t aRouterId);

    /**
     * This method returns the RLOC16 for a given EID, or initiates an Address Query if the mapping is not known.
     *
     * @param[in]   aEid     A reference to the EID.
     * @param[out]  aRloc16  The RLOC16 corresponding to @p aEid.
     *
     * @retval kTheradError_None          Successfully provided the RLOC16.
     * @retval kThreadError_AddressQuery  Initiated an Address Query.
     *
     */
    ThreadError Resolve(const Ip6::Address &aEid, Mac::ShortAddress &aRloc16);

private:
    enum
    {
        kCacheEntries = OPENTHREAD_CONFIG_ADDRESS_CACHE_ENTRIES,
        kStateUpdatePeriod = 1000u,           ///< State update period in milliseconds.
    };

    /**
     * Thread Protocol Parameters and Constants
     *
     */
    enum
    {
        kAddressQueryTimeout = 3,             ///< ADDRESS_QUERY_TIMEOUT (seconds)
        kAddressQueryInitialRetryDelay = 15,  ///< ADDRESS_QUERY_INITIAL_RETRY_DELAY (seconds)
        kAddressQueryMaxRetryDelay = 480,     ///< ADDRESS_QUERY_MAX_RETRY_DELAY (seconds)
    };

    struct Cache
    {
        Ip6::Address mTarget;
        uint8_t mIid[Ip6::Address::kInterfaceIdentifierSize];
        Mac::ShortAddress mRloc16;
        uint8_t mTimeout;
        uint8_t mFailureCount : 4;
        enum State
        {
            kStateInvalid = 0,
            kStateDiscover = 1,
            kStateRetry = 2,
            kStateValid = 3,
        };
        State mState : 2;
    };

    ThreadError SendAddressQuery(const Ip6::Address &aEid);
    ThreadError SendAddressError(const ThreadTargetTlv &aTarget, const ThreadMeshLocalEidTlv &aEid,
                                 const Ip6::Address *aDestination);
    void SendAddressQueryResponse(const ThreadTargetTlv &aTargetTlv, const ThreadMeshLocalEidTlv &aMlEidTlv,
                                  const ThreadLastTransactionTimeTlv *aLastTransactionTimeTlv,
                                  const Ip6::Address &aDestination);
    void SendAddressNotificationResponse(const Coap::Header &aRequestHeader, const Ip6::MessageInfo &aMessageInfo);

    static void HandleUdpReceive(void *aContext, otMessage aMessage, const otMessageInfo *aMessageInfo);

    static void HandleAddressError(void *aContext, Coap::Header &aHeader,
                                   Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    void HandleAddressError(Coap::Header &aHeader, Message &aMessage, const Ip6::MessageInfo &aMessageInfo);

    static void HandleAddressQuery(void *aContext, Coap::Header &aHeader,
                                   Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    void HandleAddressQuery(Coap::Header &aHeader, Message &aMessage, const Ip6::MessageInfo &aMessageInfo);

    static void HandleAddressNotification(void *aContext, Coap::Header &aHeader,
                                          Message &aMessage, const Ip6::MessageInfo &aMessageInfo);
    void HandleAddressNotification(Coap::Header &aHeader, Message &aMessage, const Ip6::MessageInfo &aMessageInfo);

    static void HandleDstUnreach(void *aContext, Message &aMessage, const Ip6::MessageInfo &aMessageInfo,
                                 const Ip6::IcmpHeader &aIcmpHeader);
    void HandleDstUnreach(Message &aMessage, const Ip6::MessageInfo &aMessageInfo, const Ip6::IcmpHeader &aIcmpHeader);

    static void HandleTimer(void *aContext);
    void HandleTimer(void);

    Coap::Resource mAddressError;
    Coap::Resource mAddressQuery;
    Coap::Resource mAddressNotification;
    Cache mCache[kCacheEntries];
    uint16_t mCoapMessageId;
    uint8_t mCoapToken[2];
    Ip6::IcmpHandler mIcmpHandler;
    Ip6::UdpSocket mSocket;
    Timer mTimer;

    MeshForwarder *mMeshForwarder;
    Coap::Server *mCoapServer;
    Mle::MleRouter *mMle;
    Ip6::Netif *mNetif;
};

/**
 * @}
 */

}  // namespace Thread

#endif  // ADDRESS_RESOLVER_HPP_
