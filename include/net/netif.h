/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#ifndef NET_NETIF_H_
#define NET_NETIF_H_

#include <common/message.h>
#include <common/tasklet.h>
#include <mac/mac_frame.h>
#include <net/ip6_address.h>
#include <net/socket.h>

namespace Thread {

class LinkAddress
{
public :
    enum HardwareType
    {
        kEui64 = 27,
    };
    HardwareType type;
    uint8_t length;
    Mac::Address64 address64;
};

class NetifUnicastAddress
{
    friend class Netif;

public:
    const NetifUnicastAddress *GetNext() const { return m_next; }

    Ip6Address address;
    uint32_t preferred_lifetime;
    uint32_t valid_lifetime;
    uint8_t prefix_length;

private:
    NetifUnicastAddress *m_next;
};

class NetifMulticastAddress
{
    friend class Netif;

public:
    const NetifMulticastAddress *GetNext() const { return m_next; }

    Ip6Address address;

private:
    NetifMulticastAddress *m_next;
};

class NetifHandler
{
    friend class Netif;

public:
    typedef void (*UnicastAddressesChangedHandler)(void *context);
    NetifHandler(UnicastAddressesChangedHandler handler, void *context) {
        m_unicast_handler = handler;
        m_context = context;
    }

private:
    void HandleUnicastAddressesChanged() { m_unicast_handler(m_context); }

    UnicastAddressesChangedHandler m_unicast_handler;
    void *m_context;
    NetifHandler *m_next;
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
                                    uint8_t *prefix_match) = 0;

    static Netif *GetNetifList();
    static Netif *GetNetifById(uint8_t interface_id);
    static Netif *GetNetifByName(char *name);
    static bool IsUnicastAddress(const Ip6Address &address);
    static const NetifUnicastAddress *SelectSourceAddress(Ip6MessageInfo &message_info);
    static int GetOnLinkNetif(const Ip6Address &address);

private:
    static void HandleUnicastChangedTask(void *context);
    void HandleUnicastChangedTask();

    NetifHandler *m_handlers = NULL;
    NetifUnicastAddress *m_unicast_addresses = NULL;
    NetifMulticastAddress *m_multicast_addresses = NULL;
    int m_interface_id = -1;
    bool m_all_routers_subscribed = false;
    Tasklet m_unicast_changed_task;
    Netif *m_next = NULL;

    static Netif *s_netif_list_head;
    static int s_next_interface_id;
};

}  // namespace Thread

#endif  // NET_NETIF_H_
