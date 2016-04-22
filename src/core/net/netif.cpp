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
 *   This file implements IPv6 network interfaces.
 */

#include <common/code_utils.hpp>
#include <common/debug.hpp>
#include <common/message.hpp>
#include <net/netif.hpp>

namespace Thread {

Netif *Netif::sNetifListHead = NULL;
int Netif::sNextInterfaceId = 1;

Netif::Netif() :
    mUnicastChangedTask(&HandleUnicastChangedTask, this)
{
}

ThreadError Netif::RegisterHandler(NetifHandler &handler)
{
    ThreadError error = kThreadError_None;

    for (NetifHandler *cur = mHandlers; cur; cur = cur->mNext)
    {
        if (cur == &handler)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    handler.mNext = mHandlers;
    mHandlers = &handler;

exit:
    return error;
}

ThreadError Netif::AddNetif()
{
    ThreadError error = kThreadError_None;
    Netif *netif;

    if (sNetifListHead == NULL)
    {
        sNetifListHead = this;
    }
    else
    {
        netif = sNetifListHead;

        do
        {
            if (netif == this)
            {
                ExitNow(error = kThreadError_Busy);
            }
        }
        while (netif->mNext);

        netif->mNext = this;
    }

    mNext = NULL;

    if (mInterfaceId < 0)
    {
        mInterfaceId = sNextInterfaceId++;
    }

exit:
    return error;
}

ThreadError Netif::RemoveNetif()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(sNetifListHead != NULL, error = kThreadError_Busy);

    if (sNetifListHead == this)
    {
        sNetifListHead = mNext;
    }
    else
    {
        for (Netif *netif = sNetifListHead; netif->mNext; netif = netif->mNext)
        {
            if (netif->mNext != this)
            {
                continue;
            }

            netif->mNext = mNext;
            break;
        }
    }

    mNext = NULL;

exit:
    return error;
}

Netif *Netif::GetNext() const
{
    return mNext;
}

Netif *Netif::GetNetifById(uint8_t interfaceId)
{
    Netif *netif;

    for (netif = sNetifListHead; netif; netif = netif->mNext)
    {
        if (netif->mInterfaceId == interfaceId)
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

    for (netif = sNetifListHead; netif; netif = netif->mNext)
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
    return mInterfaceId;
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
        ExitNow(rval = mAllRoutersSubscribed);
    }

    for (NetifMulticastAddress *cur = mMulticastAddresses; cur; cur = cur->mNext)
    {
        if (memcmp(&cur->mAddress, &address, sizeof(cur->mAddress)) == 0)
        {
            ExitNow(rval = true);
        }
    }

exit:
    return rval;
}

ThreadError Netif::SubscribeAllRoutersMulticast()
{
    mAllRoutersSubscribed = true;
    return kThreadError_None;
}

ThreadError Netif::UnsubscribeAllRoutersMulticast()
{
    mAllRoutersSubscribed = false;
    return kThreadError_None;
}

ThreadError Netif::SubscribeMulticast(NetifMulticastAddress &address)
{
    ThreadError error = kThreadError_None;

    for (NetifMulticastAddress *cur = mMulticastAddresses; cur; cur = cur->mNext)
    {
        if (cur == &address)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    address.mNext = mMulticastAddresses;
    mMulticastAddresses = &address;

exit:
    return error;
}

ThreadError Netif::UnsubscribeMulticast(const NetifMulticastAddress &address)
{
    ThreadError error = kThreadError_None;

    if (mMulticastAddresses == &address)
    {
        mMulticastAddresses = mMulticastAddresses->mNext;
        ExitNow();
    }
    else if (mMulticastAddresses != NULL)
    {
        for (NetifMulticastAddress *cur = mMulticastAddresses; cur->mNext; cur = cur->mNext)
        {
            if (cur->mNext == &address)
            {
                cur->mNext = address.mNext;
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
    return mUnicastAddresses;
}

ThreadError Netif::AddUnicastAddress(NetifUnicastAddress &address)
{
    ThreadError error = kThreadError_None;

    for (NetifUnicastAddress *cur = mUnicastAddresses; cur; cur = cur->GetNext())
    {
        if (cur == &address)
        {
            ExitNow(error = kThreadError_Busy);
        }
    }

    address.mNext = mUnicastAddresses;
    mUnicastAddresses = &address;

    mUnicastChangedTask.Post();

exit:
    return error;
}

ThreadError Netif::RemoveUnicastAddress(const NetifUnicastAddress &address)
{
    ThreadError error = kThreadError_None;

    if (mUnicastAddresses == &address)
    {
        mUnicastAddresses = mUnicastAddresses->GetNext();
        ExitNow();
    }
    else if (mUnicastAddresses != NULL)
    {
        for (NetifUnicastAddress *cur = mUnicastAddresses; cur->GetNext(); cur = cur->GetNext())
        {
            if (cur->mNext == &address)
            {
                cur->mNext = address.mNext;
                ExitNow();
            }
        }
    }

    ExitNow(error = kThreadError_Error);

exit:
    mUnicastChangedTask.Post();
    return error;
}

Netif *Netif::GetNetifList()
{
    return sNetifListHead;
}

bool Netif::IsUnicastAddress(const Ip6Address &address)
{
    bool rval = false;

    for (Netif *netif = sNetifListHead; netif; netif = netif->mNext)
    {
        for (NetifUnicastAddress *cur = netif->mUnicastAddresses; cur; cur = cur->GetNext())
        {
            if (cur->GetAddress() == address)
            {
                ExitNow(rval = true);
            }
        }
    }

exit:
    return rval;
}

const NetifUnicastAddress *Netif::SelectSourceAddress(Ip6MessageInfo &messageInfo)
{
    Ip6Address *destination = &messageInfo.GetPeerAddr();
    int interfaceId = messageInfo.mInterfaceId;
    const NetifUnicastAddress *rvalAddr = NULL;
    const Ip6Address *candidateAddr;
    uint8_t candidateId;
    uint8_t rvalIface = 0;

    for (Netif *netif = GetNetifList(); netif; netif = netif->mNext)
    {
        candidateId = netif->GetInterfaceId();

        for (const NetifUnicastAddress *addr = netif->GetUnicastAddresses(); addr; addr = addr->GetNext())
        {
            candidateAddr = &addr->GetAddress();

            if (destination->IsLinkLocal() || destination->IsMulticast())
            {
                if (interfaceId != candidateId)
                {
                    continue;
                }
            }

            if (rvalAddr == NULL)
            {
                // Rule 0: Prefer any address
                rvalAddr = addr;
                rvalIface = candidateId;
            }
            else if (*candidateAddr == *destination)
            {
                // Rule 1: Prefer same address
                rvalAddr = addr;
                rvalIface = candidateId;
                goto exit;
            }
            else if (candidateAddr->GetScope() < rvalAddr->GetAddress().GetScope())
            {
                // Rule 2: Prefer appropriate scope
                if (candidateAddr->GetScope() >= destination->GetScope())
                {
                    rvalAddr = addr;
                    rvalIface = candidateId;
                }
            }
            else if (candidateAddr->GetScope() > rvalAddr->GetAddress().GetScope())
            {
                if (rvalAddr->GetAddress().GetScope() < destination->GetScope())
                {
                    rvalAddr = addr;
                    rvalIface = candidateId;
                }
            }
            else if (addr->mPreferredLifetime != 0 && rvalAddr->mPreferredLifetime == 0)
            {
                // Rule 3: Avoid deprecated addresses
                rvalAddr = addr;
                rvalIface = candidateId;
            }
            else if (messageInfo.mInterfaceId != 0 && messageInfo.mInterfaceId == candidateId &&
                     rvalIface != candidateId)
            {
                // Rule 4: Prefer home address
                // Rule 5: Prefer outgoing interface
                rvalAddr = addr;
                rvalIface = candidateId;
            }
            else if (destination->PrefixMatch(*candidateAddr) > destination->PrefixMatch(rvalAddr->GetAddress()))
            {
                // Rule 6: Prefer matching label
                // Rule 7: Prefer public address
                // Rule 8: Use longest prefix matching
                rvalAddr = addr;
                rvalIface = candidateId;
            }
        }
    }

exit:
    messageInfo.mInterfaceId = rvalIface;
    return rvalAddr;
}

int Netif::GetOnLinkNetif(const Ip6Address &address)
{
    int rval = -1;

    for (Netif *netif = sNetifListHead; netif; netif = netif->mNext)
    {
        for (NetifUnicastAddress *cur = netif->mUnicastAddresses; cur; cur = cur->GetNext())
        {
            if (cur->GetAddress().PrefixMatch(address) >= cur->mPrefixLength)
            {
                ExitNow(rval = netif->mInterfaceId);
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
    for (NetifHandler *handler = mHandlers; handler; handler = handler->mNext)
    {
        handler->HandleUnicastAddressesChanged();
    }
}

}  // namespace Thread
