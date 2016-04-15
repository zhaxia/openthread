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

#ifndef ADDRESS_RESOLVER_HPP_
#define ADDRESS_RESOLVER_HPP_

#include <coap/coap_server.hpp>
#include <common/thread_error.hpp>
#include <common/timer.hpp>
#include <mac/mac.hpp>
#include <net/icmp6.hpp>
#include <net/udp6.hpp>

namespace Thread {

class MeshForwarder;
class ThreadLastTransactionTimeTlv;
class ThreadMeshLocalIidTlv;
class ThreadNetif;
class ThreadTargetTlv;

class AddressResolver
{
public:
    class Cache
    {
    public:
        Ip6Address mTarget;
        uint8_t mIid[8];
        Mac::Address16 mRloc;
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

    explicit AddressResolver(ThreadNetif &netif);
    ThreadError Clear();
    ThreadError Remove(uint8_t routerId);
    ThreadError Resolve(const Ip6Address &eid, Mac::Address16 &rloc);

    const Cache *GetCacheEntries(uint16_t *numEntries) const;

private:
    enum
    {
        kCacheEntries = 8,
        kDiscoverTimeout = 3,  // seconds
    };

    ThreadError SendAddressQuery(const Ip6Address &eid);
    ThreadError SendAddressError(const ThreadTargetTlv &target, const ThreadMeshLocalIidTlv &eid,
                                 const Ip6Address *destination);
    void SendAddressQueryResponse(const ThreadTargetTlv &targetTlv, const ThreadMeshLocalIidTlv &mlIidTlv,
                                  const ThreadLastTransactionTimeTlv *lastTransactionTimeTlv,
                                  const Ip6Address &destination);
    void SendAddressNotificationResponse(const Coap::Header &requestHeader, const Ip6MessageInfo &messageInfo);

    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &messageInfo);

    static void HandleAddressError(void *context, Coap::Header &header,
                                   Message &message, const Ip6MessageInfo &messageInfo);
    void HandleAddressError(Coap::Header &header, Message &message, const Ip6MessageInfo &messageInfo);

    static void HandleAddressQuery(void *context, Coap::Header &header,
                                   Message &message, const Ip6MessageInfo &messageInfo);
    void HandleAddressQuery(Coap::Header &header, Message &message, const Ip6MessageInfo &messageInfo);

    static void HandleAddressNotification(void *context, Coap::Header &header,
                                          Message &message, const Ip6MessageInfo &messageInfo);
    void HandleAddressNotification(Coap::Header &header, Message &message, const Ip6MessageInfo &messageInfo);

    static void HandleDstUnreach(void *context, Message &message, const Ip6MessageInfo &messageInfo,
                                 const Icmp6Header &icmp6Header);
    void HandleDstUnreach(Message &message, const Ip6MessageInfo &messageInfo, const Icmp6Header &icmp6Header);

    static void HandleTimer(void *context);
    void HandleTimer();

    Coap::Resource mAddressError;
    Coap::Resource mAddressQuery;
    Coap::Resource mAddressNotification;
    Cache mCache[kCacheEntries];
    uint16_t mCoapMessageId;
    uint8_t mCoapToken[2];
    Icmp6Handler mIcmp6Handler;
    Udp6Socket mSocket;
    Timer mTimer;

    MeshForwarder *mMeshForwarder;
    Coap::Server *mCoapServer;
    Mle::MleRouter *mMle;
    Netif *mNetif;
};

}  // namespace Thread

#endif  // ADDRESS_RESOLVER_HPP_
