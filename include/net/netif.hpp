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

#ifndef NET_NETIF_HPP_
#define NET_NETIF_HPP_

#include <common/message.hpp>
#include <common/tasklet.hpp>
#include <mac/mac_frame.hpp>
#include <net/ip6_address.hpp>
#include <net/socket.hpp>

namespace Thread {

class LinkAddress
{
public :
    enum HardwareType
    {
        kEui64 = 27,
    };
    HardwareType mType;
    uint8_t mLength;
    Mac::Address64 mAddress64;
};

class NetifUnicastAddress
{
    friend class Netif;

public:
    const NetifUnicastAddress *GetNext() const { return mNext; }

    Ip6Address mAddress;
    uint32_t mPreferredLifetime;
    uint32_t mValidLifetime;
    uint8_t mPrefixLength;

private:
    NetifUnicastAddress *mNext;
};

class NetifMulticastAddress
{
    friend class Netif;

public:
    const NetifMulticastAddress *GetNext() const { return mNext; }

    Ip6Address mAddress;

private:
    NetifMulticastAddress *mNext;
};

class NetifHandler
{
    friend class Netif;

public:
    typedef void (*UnicastAddressesChangedHandler)(void *context);
    NetifHandler(UnicastAddressesChangedHandler handler, void *context) {
        mUnicastHandler = handler;
        mContext = context;
    }

private:
    void HandleUnicastAddressesChanged() { mUnicastHandler(mContext); }

    UnicastAddressesChangedHandler mUnicastHandler;
    void *mContext;
    NetifHandler *mNext;
};

class Netif
{
public:
    Netif();
    ThreadError AddNetif();
    ThreadError RemoveNetif();
    Netif *GetNext() const;
    int GetInterfaceId() const;
    const NetifUnicastAddress *GetUnicastAddresses() const;
    ThreadError AddUnicastAddress(NetifUnicastAddress &address);
    ThreadError RemoveUnicastAddress(const NetifUnicastAddress &address);

    bool IsMulticastSubscribed(const Ip6Address &address) const;
    ThreadError SubscribeAllRoutersMulticast();
    ThreadError UnsubscribeAllRoutersMulticast();
    ThreadError SubscribeMulticast(NetifMulticastAddress &address);
    ThreadError UnsubscribeMulticast(const NetifMulticastAddress &address);

    ThreadError RegisterHandler(NetifHandler &handler);

    virtual ThreadError SendMessage(Message &message) = 0;
    virtual const char *GetName() const = 0;
    virtual ThreadError GetLinkAddress(LinkAddress &address) const = 0;
    virtual ThreadError RouteLookup(const Ip6Address &source, const Ip6Address &destination,
                                    uint8_t *prefixMatch) = 0;

    static Netif *GetNetifList();
    static Netif *GetNetifById(uint8_t interfaceId);
    static Netif *GetNetifByName(char *name);
    static bool IsUnicastAddress(const Ip6Address &address);
    static const NetifUnicastAddress *SelectSourceAddress(Ip6MessageInfo &messageInfo);
    static int GetOnLinkNetif(const Ip6Address &address);

private:
    static void HandleUnicastChangedTask(void *context);
    void HandleUnicastChangedTask();

    NetifHandler *mHandlers = NULL;
    NetifUnicastAddress *mUnicastAddresses = NULL;
    NetifMulticastAddress *mMulticastAddresses = NULL;
    int mInterfaceId = -1;
    bool mAllRoutersSubscribed = false;
    Tasklet mUnicastChangedTask;
    Netif *mNext = NULL;

    static Netif *sNetifListHead;
    static int sNextInterfaceId;
};

}  // namespace Thread

#endif  // NET_NETIF_HPP_
