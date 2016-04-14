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

#include <common/code_utils.hpp>
#include <common/debug.hpp>
#include <common/message.hpp>
#include <net/netif.hpp>

namespace Thread {

Netif *Netif::s_netif_list_head = NULL;
int Netif::s_next_interface_id = 1;

Netif::Netif() :
    m_unicast_changed_task(&HandleUnicastChangedTask, this)
{
}

ThreadError Netif::RegisterHandler(NetifHandler &handler)
{
    ThreadError error = kThreadError_None;

    for (NetifHandler *cur = m_handlers; cur; cur = cur->m_next)
    {
        if (cur == &handler)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    handler.m_next = m_handlers;
    m_handlers = &handler;

exit:
    return error;
}

ThreadError Netif::AddNetif()
{
    ThreadError error = kThreadError_None;
    Netif *netif;

    if (s_netif_list_head == NULL)
    {
        s_netif_list_head = this;
    }
    else
    {
        netif = s_netif_list_head;

        do
        {
            if (netif == this)
            {
                ExitNow(error = kThreadError_Busy);
            }
        }
        while (netif->m_next);

        netif->m_next = this;
    }

    m_next = NULL;

    if (m_interface_id < 0)
    {
        m_interface_id = s_next_interface_id++;
    }

exit:
    return error;
}

ThreadError Netif::RemoveNetif()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(s_netif_list_head != NULL, error = kThreadError_Busy);

    if (s_netif_list_head == this)
    {
        s_netif_list_head = m_next;
    }
    else
    {
        for (Netif *netif = s_netif_list_head; netif->m_next; netif = netif->m_next)
        {
            if (netif->m_next != this)
            {
                continue;
            }

            netif->m_next = m_next;
            break;
        }
    }

    m_next = NULL;

exit:
    return error;
}

Netif *Netif::GetNext() const
{
    return m_next;
}

Netif *Netif::GetNetifById(uint8_t interface_id)
{
    Netif *netif;

    for (netif = s_netif_list_head; netif; netif = netif->m_next)
    {
        if (netif->m_interface_id == interface_id)
        {
            ExitNow();
        }
    }

exit:
    return netif;
}

Netif *Netif::GetNetifByName(char *name)
{
    Netif *netif;

    for (netif = s_netif_list_head; netif; netif = netif->m_next)
    {
        if (strcmp(netif->GetName(), name) == 0)
        {
            ExitNow();
        }
    }

exit:
    return netif;
}

int Netif::GetInterfaceId() const
{
    return m_interface_id;
}

bool Netif::IsMulticastSubscribed(const Ip6Address &address) const
{
    bool rval = false;

    if (address.IsLinkLocalAllNodesMulticast() || address.IsRealmLocalAllNodesMulticast())
    {
        ExitNow(rval = true);
    }
    else if (address.IsLinkLocalAllRoutersMulticast() || address.IsRealmLocalAllRoutersMulticast())
    {
        ExitNow(rval = m_all_routers_subscribed);
    }

    for (NetifMulticastAddress *cur = m_multicast_addresses; cur; cur = cur->m_next)
    {
        if (memcmp(&cur->address, &address, sizeof(cur->address)) == 0)
        {
            ExitNow(rval = true);
        }
    }

exit:
    return rval;
}

ThreadError Netif::SubscribeAllRoutersMulticast()
{
    m_all_routers_subscribed = true;
    return kThreadError_None;
}

ThreadError Netif::UnsubscribeAllRoutersMulticast()
{
    m_all_routers_subscribed = false;
    return kThreadError_None;
}

ThreadError Netif::SubscribeMulticast(NetifMulticastAddress &address)
{
    ThreadError error = kThreadError_None;

    for (NetifMulticastAddress *cur = m_multicast_addresses; cur; cur = cur->m_next)
    {
        if (cur == &address)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    address.m_next = m_multicast_addresses;
    m_multicast_addresses = &address;

exit:
    return error;
}

ThreadError Netif::UnsubscribeMulticast(const NetifMulticastAddress &address)
{
    ThreadError error = kThreadError_None;

    if (m_multicast_addresses == &address)
    {
        m_multicast_addresses = m_multicast_addresses->m_next;
        ExitNow();
    }
    else if (m_multicast_addresses != NULL)
    {
        for (NetifMulticastAddress *cur = m_multicast_addresses; cur->m_next; cur = cur->m_next)
        {
            if (cur->m_next == &address)
            {
                cur->m_next = address.m_next;
                ExitNow();
            }
        }
    }

    ExitNow(error = kThreadError_Error);

exit:
    return error;
}

const NetifUnicastAddress *Netif::GetUnicastAddresses() const
{
    return m_unicast_addresses;
}

ThreadError Netif::AddUnicastAddress(NetifUnicastAddress &address)
{
    ThreadError error = kThreadError_None;

    for (NetifUnicastAddress *cur = m_unicast_addresses; cur; cur = cur->m_next)
    {
        if (cur == &address)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    address.m_next = m_unicast_addresses;
    m_unicast_addresses = &address;

    m_unicast_changed_task.Post();

exit:
    return error;
}

ThreadError Netif::RemoveUnicastAddress(const NetifUnicastAddress &address)
{
    ThreadError error = kThreadError_None;

    if (m_unicast_addresses == &address)
    {
        m_unicast_addresses = m_unicast_addresses->m_next;
        ExitNow();
    }
    else if (m_unicast_addresses != NULL)
    {
        for (NetifUnicastAddress *cur = m_unicast_addresses; cur->m_next; cur = cur->m_next)
        {
            if (cur->m_next == &address)
            {
                cur->m_next = address.m_next;
                ExitNow();
            }
        }
    }

    ExitNow(error = kThreadError_Error);

exit:
    m_unicast_changed_task.Post();
    return error;
}

Netif *Netif::GetNetifList()
{
    return s_netif_list_head;
}

bool Netif::IsUnicastAddress(const Ip6Address &address)
{
    bool rval = false;

    for (Netif *netif = s_netif_list_head; netif; netif = netif->m_next)
    {
        for (NetifUnicastAddress *cur = netif->m_unicast_addresses; cur; cur = cur->m_next)
        {
            if (cur->address == address)
            {
                ExitNow(rval = true);
            }
        }
    }

exit:
    return rval;
}

const NetifUnicastAddress *Netif::SelectSourceAddress(Ip6MessageInfo &message_info)
{
    Ip6Address *destination = &message_info.peer_addr;
    int interface_id = message_info.interface_id;
    const NetifUnicastAddress *rval_addr = NULL;
    const Ip6Address *candidate_addr;
    uint8_t candidate_id;
    uint8_t rval_iface = 0;

    for (Netif *netif = GetNetifList(); netif; netif = netif->m_next)
    {
        candidate_id = netif->GetInterfaceId();

        for (const NetifUnicastAddress *addr = netif->GetUnicastAddresses(); addr; addr = addr->m_next)
        {
            candidate_addr = &addr->address;

            if (destination->IsLinkLocal() || destination->IsMulticast())
            {
                if (interface_id != candidate_id)
                {
                    continue;
                }
            }

            if (rval_addr == NULL)
            {
                // Rule 0: Prefer any address
                rval_addr = addr;
                rval_iface = candidate_id;
            }
            else if (*candidate_addr == *destination)
            {
                // Rule 1: Prefer same address
                rval_addr = addr;
                rval_iface = candidate_id;
                goto exit;
            }
            else if (candidate_addr->GetScope() < rval_addr->address.GetScope())
            {
                // Rule 2: Prefer appropriate scope
                if (candidate_addr->GetScope() >= destination->GetScope())
                {
                    rval_addr = addr;
                    rval_iface = candidate_id;
                }
            }
            else if (candidate_addr->GetScope() > rval_addr->address.GetScope())
            {
                if (rval_addr->address.GetScope() < destination->GetScope())
                {
                    rval_addr = addr;
                    rval_iface = candidate_id;
                }
            }
            else if (addr->preferred_lifetime != 0 && rval_addr->preferred_lifetime == 0)
            {
                // Rule 3: Avoid deprecated addresses
                rval_addr = addr;
                rval_iface = candidate_id;
            }
            else if (message_info.interface_id != 0 && message_info.interface_id == candidate_id &&
                     rval_iface != candidate_id)
            {
                // Rule 4: Prefer home address
                // Rule 5: Prefer outgoing interface
                rval_addr = addr;
                rval_iface = candidate_id;
            }
            else if (destination->PrefixMatch(*candidate_addr) > destination->PrefixMatch(rval_addr->address))
            {
                // Rule 6: Prefer matching label
                // Rule 7: Prefer public address
                // Rule 8: Use longest prefix matching
                rval_addr = addr;
                rval_iface = candidate_id;
            }
        }
    }

exit:
    message_info.interface_id = rval_iface;
    return rval_addr;
}

int Netif::GetOnLinkNetif(const Ip6Address &address)
{
    int rval = -1;

    for (Netif *netif = s_netif_list_head; netif; netif = netif->m_next)
    {
        for (NetifUnicastAddress *cur = netif->m_unicast_addresses; cur; cur = cur->m_next)
        {
            if (cur->address.PrefixMatch(address) >= cur->prefix_length)
            {
                ExitNow(rval = netif->m_interface_id);
            }
        }
    }

exit:
    return rval;
}

void Netif::HandleUnicastChangedTask(void *context)
{
    Netif *obj = reinterpret_cast<Netif *>(context);
    obj->HandleUnicastChangedTask();
}

void Netif::HandleUnicastChangedTask()
{
    for (NetifHandler *handler = m_handlers; handler; handler = handler->m_next)
    {
        handler->HandleUnicastAddressesChanged();
    }
}

}  // namespace Thread
