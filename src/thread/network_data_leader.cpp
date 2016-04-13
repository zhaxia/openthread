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

#include <thread/network_data_leader.h>
#include <coap/coap_header.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/message.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <common/timer.h>
#include <mac/mac_frame.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>
#include <thread/thread_tlvs.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace NetworkData {

Leader::Leader(ThreadNetif &netif):
    m_server_data("n/sd", &HandleServerData, this),
    m_timer(&HandleTimer, this)
{
    m_coap_server = netif.GetCoapServer();
    m_netif = &netif;
    m_mle = netif.GetMle();
}

ThreadError Leader::Init()
{
    memset(addresses_, 0, sizeof(addresses_));
    memset(m_context_last_used, 0, sizeof(m_context_last_used));
    version_ = Random::Get();
    stable_version_ = Random::Get();
    m_length = 0;
    return kThreadError_None;
}

ThreadError Leader::Start()
{
    return m_coap_server->AddResource(m_server_data);
}

ThreadError Leader::Stop()
{
    return kThreadError_None;
}

uint8_t Leader::GetVersion() const
{
    return version_;
}

uint8_t Leader::GetStableVersion() const
{
    return stable_version_;
}

uint32_t Leader::GetContextIdReuseDelay() const
{
    return m_context_id_reuse_delay;
}

ThreadError Leader::SetContextIdReuseDelay(uint32_t delay)
{
    m_context_id_reuse_delay = delay;
    return kThreadError_None;
}

ThreadError Leader::GetContext(const Ip6Address &address, Context &context)
{
    PrefixTlv *prefix;
    ContextTlv *context_tlv;

    context.prefix_length = 0;

    if (PrefixMatch(m_mle->GetMeshLocalPrefix(), address.addr8, 64) >= 0)
    {
        context.prefix = m_mle->GetMeshLocalPrefix();
        context.prefix_length = 64;
        context.context_id = 0;
    }

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);

        if (PrefixMatch(prefix->GetPrefix(), address.addr8, prefix->GetPrefixLength()) < 0)
        {
            continue;
        }

        context_tlv = FindContext(*prefix);

        if (context_tlv == NULL)
        {
            continue;
        }

        if (prefix->GetPrefixLength() > context.prefix_length)
        {
            context.prefix = prefix->GetPrefix();
            context.prefix_length = prefix->GetPrefixLength();
            context.context_id = context_tlv->GetContextId();
        }
    }

    return (context.prefix_length > 0) ? kThreadError_None : kThreadError_Error;
}

ThreadError Leader::GetContext(uint8_t context_id, Context &context)
{
    ThreadError error = kThreadError_Error;
    PrefixTlv *prefix;
    ContextTlv *context_tlv;

    if (context_id == 0)
    {
        context.prefix = m_mle->GetMeshLocalPrefix();
        context.prefix_length = 64;
        context.context_id = 0;
        ExitNow(error = kThreadError_None);
    }

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);
        context_tlv = FindContext(*prefix);

        if (context_tlv == NULL)
        {
            continue;
        }

        if (context_tlv->GetContextId() != context_id)
        {
            continue;
        }

        context.prefix = prefix->GetPrefix();
        context.prefix_length = prefix->GetPrefixLength();
        context.context_id = context_tlv->GetContextId();
        ExitNow(error = kThreadError_None);
    }

exit:
    return error;
}

ThreadError Leader::ConfigureAddresses()
{
    PrefixTlv *prefix;

    // clear out addresses that are not on-mesh
    for (size_t i = 0; i < sizeof(addresses_) / sizeof(addresses_[0]); i++)
    {
        if (addresses_[i].valid_lifetime == 0 ||
            IsOnMesh(addresses_[i].address))
        {
            continue;
        }

        m_netif->RemoveUnicastAddress(addresses_[i]);
        addresses_[i].valid_lifetime = 0;
    }

    // configure on-mesh addresses
    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);
        ConfigureAddress(*prefix);
    }

    return kThreadError_None;
}

ThreadError Leader::ConfigureAddress(PrefixTlv &prefix)
{
    BorderRouterTlv *border_router;
    BorderRouterEntry *entry;

    // look for Border Router TLV
    if ((border_router = FindBorderRouter(prefix)) == NULL)
    {
        ExitNow();
    }

    // check if Valid flag is set
    if ((entry = border_router->GetEntry(0)) == NULL ||
        entry->IsValid() == false)
    {
        ExitNow();
    }

    // check if address is already added for this prefix
    for (size_t i = 0; i < sizeof(addresses_) / sizeof(addresses_[0]); i++)
    {
        if (addresses_[i].valid_lifetime != 0 &&
            addresses_[i].prefix_length == prefix.GetPrefixLength() &&
            PrefixMatch(addresses_[i].address.addr8, prefix.GetPrefix(), prefix.GetPrefixLength()) >= 0)
        {
            addresses_[i].preferred_lifetime = entry->IsPreferred() ? 0xffffffff : 0;
            ExitNow();
        }
    }

    // configure address for this prefix
    for (size_t i = 0; i < sizeof(addresses_) / sizeof(addresses_[0]); i++)
    {
        if (addresses_[i].valid_lifetime != 0)
        {
            continue;
        }

        memset(&addresses_[i], 0, sizeof(addresses_[i]));
        memcpy(addresses_[i].address.addr8, prefix.GetPrefix(), (prefix.GetPrefixLength() + 7) / 8);

        for (size_t j = 8; j < sizeof(addresses_[i].address); j++)
        {
            addresses_[i].address.addr8[j] = Random::Get();
        }

        addresses_[i].prefix_length = prefix.GetPrefixLength();
        addresses_[i].preferred_lifetime = entry->IsPreferred() ? 0xffffffff : 0;
        addresses_[i].valid_lifetime = 0xffffffff;
        m_netif->AddUnicastAddress(addresses_[i]);
        break;
    }

exit:
    return kThreadError_None;
}

ContextTlv *Leader::FindContext(PrefixTlv &prefix)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeContext)
        {
            return reinterpret_cast<ContextTlv *>(cur);
        }

        cur = cur->GetNext();
    }

    return NULL;
}

bool Leader::IsOnMesh(const Ip6Address &address)
{
    PrefixTlv *prefix;
    bool rval = false;

    if (memcmp(address.addr8, m_mle->GetMeshLocalPrefix(), 8) == 0)
    {
        ExitNow(rval = true);
    }

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);

        if (PrefixMatch(prefix->GetPrefix(), address.addr8, prefix->GetPrefixLength()) < 0)
        {
            continue;
        }

        if (FindBorderRouter(*prefix) == NULL)
        {
            continue;
        }

        ExitNow(rval = true);
    }

exit:
    return rval;
}

ThreadError Leader::RouteLookup(const Ip6Address &source, const Ip6Address &destination,
                                uint8_t *prefix_match, uint16_t *rloc)
{
    ThreadError error = kThreadError_NoRoute;
    PrefixTlv *prefix;

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);

        if (PrefixMatch(prefix->GetPrefix(), source.addr8, prefix->GetPrefixLength()) >= 0)
        {
            if (ExternalRouteLookup(prefix->GetDomainId(), destination, prefix_match, rloc) == kThreadError_None)
            {
                ExitNow(error = kThreadError_None);
            }

            if (DefaultRouteLookup(*prefix, rloc) == kThreadError_None)
            {
                if (prefix_match)
                {
                    *prefix_match = 0;
                }

                ExitNow(error = kThreadError_None);
            }
        }
    }

exit:
    return error;
}

ThreadError Leader::ExternalRouteLookup(uint8_t domain_id, const Ip6Address &destination,
                                        uint8_t *prefix_match, uint16_t *rloc)
{
    ThreadError error = kThreadError_NoRoute;
    PrefixTlv *prefix;
    HasRouteTlv *has_route;
    HasRouteEntry *entry;
    HasRouteEntry *rval_route = NULL;
    int8_t rval_plen = 0;
    int8_t plen;
    NetworkDataTlv *cur;

    for (cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);

        if (prefix->GetDomainId() != domain_id)
        {
            continue;
        }

        plen = PrefixMatch(prefix->GetPrefix(), destination.addr8, prefix->GetPrefixLength());

        if (plen > rval_plen)
        {
            // select border router
            for (cur = reinterpret_cast<NetworkDataTlv *>(prefix->GetSubTlvs());
                 cur < reinterpret_cast<NetworkDataTlv *>(prefix->GetSubTlvs() + prefix->GetSubTlvsLength());
                 cur = cur->GetNext())
            {
                if (cur->GetType() != NetworkDataTlv::kTypeHasRoute)
                {
                    continue;
                }

                has_route = reinterpret_cast<HasRouteTlv *>(cur);

                for (int i = 0; i < has_route->GetNumEntries(); i++)
                {
                    entry = has_route->GetEntry(i);

                    if (rval_route == NULL ||
                        entry->GetPreference() > rval_route->GetPreference() ||
                        (entry->GetPreference() == rval_route->GetPreference() &&
                         m_mle->GetRouteCost(entry->GetRloc()) < m_mle->GetRouteCost(rval_route->GetRloc())))
                    {
                        rval_route = entry;
                        rval_plen = plen;
                    }
                }

            }
        }
    }

    if (rval_route != NULL)
    {
        if (rloc != NULL)
        {
            *rloc = rval_route->GetRloc();
        }

        if (prefix_match != NULL)
        {
            *prefix_match = rval_plen;
        }

        error = kThreadError_None;
    }

    return error;
}

ThreadError Leader::DefaultRouteLookup(PrefixTlv &prefix, uint16_t *rloc)
{
    ThreadError error = kThreadError_NoRoute;
    BorderRouterTlv *border_router;
    BorderRouterEntry *entry;
    BorderRouterEntry *route = NULL;

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
         cur < reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypeBorderRouter)
        {
            continue;
        }

        border_router = reinterpret_cast<BorderRouterTlv *>(cur);

        for (int i = 0; i < border_router->GetNumEntries(); i++)
        {
            entry = border_router->GetEntry(i);

            if (entry->IsDefaultRoute() == false)
            {
                continue;
            }

            if (route == NULL ||
                entry->GetPreference() > route->GetPreference() ||
                (entry->GetPreference() == route->GetPreference() &&
                 m_mle->GetRouteCost(entry->GetRloc()) < m_mle->GetRouteCost(route->GetRloc())))
            {
                route = entry;
            }
        }
    }

    if (route != NULL)
    {
        if (rloc != NULL)
        {
            *rloc = route->GetRloc();
        }

        error = kThreadError_None;
    }

    return error;
}

ThreadError Leader::SetNetworkData(uint8_t version, uint8_t stable_version, bool stable,
                                   const uint8_t *data, uint8_t data_length)
{
    version_ = version;
    stable_version_ = stable_version;
    memcpy(m_tlvs, data, data_length);
    m_length = data_length;

    if (stable)
    {
        RemoveTemporaryData(m_tlvs, m_length);
    }

    dump("set network data", m_tlvs, m_length);

    ConfigureAddresses();
    m_mle->HandleNetworkDataUpdate();

    return kThreadError_None;
}

ThreadError Leader::RemoveBorderRouter(uint16_t rloc)
{
    RemoveRloc(rloc);
    ConfigureAddresses();
    m_mle->HandleNetworkDataUpdate();
    return kThreadError_None;
}

void Leader::HandleServerData(void *context, Coap::Header &header, Message &message,
                              const Ip6MessageInfo &message_info)
{
    Leader *obj = reinterpret_cast<Leader *>(context);
    obj->HandleServerData(header, message, message_info);
}

void Leader::HandleServerData(Coap::Header &header, Message &message,
                              const Ip6MessageInfo &message_info)
{
    uint8_t tlvs_length;
    uint8_t tlvs[256];
    uint16_t rloc16;

    dprintf("Received network data registration\n");

    tlvs_length = message.GetLength() - message.GetOffset();

    message.Read(message.GetOffset(), tlvs_length, tlvs);
    rloc16 = HostSwap16(message_info.peer_addr.addr16[7]);
    RegisterNetworkData(rloc16, tlvs, tlvs_length);

    SendServerDataResponse(header, message_info, tlvs, tlvs_length);
}

void Leader::SendServerDataResponse(const Coap::Header &request_header, const Ip6MessageInfo &message_info,
                                    const uint8_t *tlvs, uint8_t tlvs_length)
{
    ThreadError error = kThreadError_None;
    Coap::Header response_header;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    response_header.Init();
    response_header.SetVersion(1);
    response_header.SetType(Coap::Header::kTypeAcknowledgment);
    response_header.SetCode(Coap::Header::kCodeChanged);
    response_header.SetMessageId(request_header.GetMessageId());
    response_header.SetToken(request_header.GetToken(), request_header.GetTokenLength());
    response_header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    response_header.Finalize();
    SuccessOrExit(error = message->Append(response_header.GetBytes(), response_header.GetLength()));
    SuccessOrExit(error = message->Append(tlvs, tlvs_length));

    SuccessOrExit(error = m_coap_server->SendMessage(*message, message_info));

    dprintf("Sent network data registration acknowledgment\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError Leader::RegisterNetworkData(uint16_t rloc, uint8_t *tlvs, uint8_t tlvs_length)
{
    ThreadError error = kThreadError_None;

    SuccessOrExit(error = RemoveRloc(rloc));
    SuccessOrExit(error = AddNetworkData(tlvs, tlvs_length));

    version_++;
    stable_version_++;

    ConfigureAddresses();
    m_mle->HandleNetworkDataUpdate();

exit:
    return error;
}

ThreadError Leader::AddNetworkData(uint8_t *tlvs, uint8_t tlvs_length)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(tlvs);
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(tlvs + tlvs_length);

    while (cur < end)
    {
        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
            AddPrefix(*reinterpret_cast<PrefixTlv *>(cur));
            dump("add prefix done", m_tlvs, m_length);
            break;

        default:
            assert(false);
            break;
        }

        cur = cur->GetNext();
    }

    dump("add done", m_tlvs, m_length);

    return kThreadError_None;
}

ThreadError Leader::AddPrefix(PrefixTlv &prefix)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

    while (cur < end)
    {
        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypeHasRoute:
            AddHasRoute(prefix, *reinterpret_cast<HasRouteTlv *>(cur));
            break;

        case NetworkDataTlv::kTypeBorderRouter:
            AddBorderRouter(prefix, *reinterpret_cast<BorderRouterTlv *>(cur));
            break;

        default:
            assert(false);
            break;
        }

        cur = cur->GetNext();
    }

    return kThreadError_None;
}

ThreadError Leader::AddHasRoute(PrefixTlv &prefix, HasRouteTlv &has_route)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *dst_prefix;
    HasRouteTlv *dst_has_route;

    if ((dst_prefix = FindPrefix(prefix.GetPrefix(), prefix.GetPrefixLength())) == NULL)
    {
        dst_prefix = reinterpret_cast<PrefixTlv *>(m_tlvs + m_length);
        Insert(reinterpret_cast<uint8_t *>(dst_prefix), sizeof(PrefixTlv) + (prefix.GetPrefixLength() + 7) / 8);
        dst_prefix->Init(prefix.GetDomainId(), prefix.GetPrefixLength(), prefix.GetPrefix());
    }

    if (has_route.IsStable())
    {
        dst_prefix->SetStable();
    }

    if ((dst_has_route = FindHasRoute(*dst_prefix, has_route.IsStable())) == NULL)
    {
        dst_has_route = reinterpret_cast<HasRouteTlv *>(dst_prefix->GetNext());
        Insert(reinterpret_cast<uint8_t *>(dst_has_route), sizeof(HasRouteTlv));
        dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(HasRouteTlv));
        dst_has_route->Init();

        if (has_route.IsStable())
        {
            dst_has_route->SetStable();
        }
    }

    Insert(reinterpret_cast<uint8_t *>(dst_has_route->GetNext()), sizeof(HasRouteEntry));
    dst_has_route->SetLength(dst_has_route->GetLength() + sizeof(HasRouteEntry));
    dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(HasRouteEntry));
    memcpy(dst_has_route->GetEntry(dst_has_route->GetNumEntries() - 1), has_route.GetEntry(0),
           sizeof(HasRouteEntry));

    return error;
}

ThreadError Leader::AddBorderRouter(PrefixTlv &prefix, BorderRouterTlv &border_router)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *dst_prefix;
    ContextTlv *dst_context;
    BorderRouterTlv *dst_border_router;
    uint8_t context_id;

    if ((dst_prefix = FindPrefix(prefix.GetPrefix(), prefix.GetPrefixLength())) == NULL)
    {
        dst_prefix = reinterpret_cast<PrefixTlv *>(m_tlvs + m_length);
        Insert(reinterpret_cast<uint8_t *>(dst_prefix), sizeof(PrefixTlv) + (prefix.GetPrefixLength() + 7) / 8);
        dst_prefix->Init(prefix.GetDomainId(), prefix.GetPrefixLength(), prefix.GetPrefix());
    }

    if (border_router.IsStable())
    {
        dst_prefix->SetStable();

        if ((dst_context = FindContext(*dst_prefix)) != NULL)
        {
            dst_context->SetCompress();
        }
        else if ((context_id = AllocateContext()) >= 0)
        {
            dst_context = reinterpret_cast<ContextTlv *>(dst_prefix->GetNext());
            Insert(reinterpret_cast<uint8_t *>(dst_context), sizeof(ContextTlv));
            dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(ContextTlv));
            dst_context->Init();
            dst_context->SetStable();
            dst_context->SetCompress();
            dst_context->SetContextId(context_id);
            dst_context->SetContextLength(prefix.GetPrefixLength());
        }
        else
        {
            ExitNow(error = kThreadError_NoBufs);
        }

        m_context_last_used[dst_context->GetContextId() - kMinContextId] = 0;
    }

    if ((dst_border_router = FindBorderRouter(*dst_prefix, border_router.IsStable())) == NULL)
    {
        dst_border_router = reinterpret_cast<BorderRouterTlv *>(dst_prefix->GetNext());
        Insert(reinterpret_cast<uint8_t *>(dst_border_router), sizeof(BorderRouterTlv));
        dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(BorderRouterTlv));
        dst_border_router->Init();

        if (border_router.IsStable())
        {
            dst_border_router->SetStable();
        }
    }

    Insert(reinterpret_cast<uint8_t *>(dst_border_router->GetNext()), sizeof(BorderRouterEntry));
    dst_border_router->SetLength(dst_border_router->GetLength() + sizeof(BorderRouterEntry));
    dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(BorderRouterEntry));
    memcpy(dst_border_router->GetEntry(dst_border_router->GetNumEntries() - 1), border_router.GetEntry(0),
           sizeof(BorderRouterEntry));

exit:
    return error;
}

int Leader::AllocateContext()
{
    int rval = -1;

    for (int i = kMinContextId; i < kMinContextId + kNumContextIds; i++)
    {
        if ((context_used_ & (1 << i)) == 0)
        {
            context_used_ |= 1 << i;
            rval = i;
            dprintf("Allocated Context ID = %d\n", rval);
            ExitNow();
        }
    }

exit:
    return rval;
}

ThreadError Leader::FreeContext(uint8_t context_id)
{
    dprintf("Free Context Id = %d\n", context_id);
    RemoveContext(context_id);
    context_used_ &= ~(1 << context_id);
    version_++;
    stable_version_++;
    m_mle->HandleNetworkDataUpdate();
    return kThreadError_None;
}

ThreadError Leader::RemoveRloc(uint16_t rloc)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
    NetworkDataTlv *end;
    PrefixTlv *prefix;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);

        if (cur >= end)
        {
            break;
        }

        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
        {
            prefix = reinterpret_cast<PrefixTlv *>(cur);
            RemoveRloc(*prefix, rloc);

            if (prefix->GetSubTlvsLength() == 0)
            {
                Remove(reinterpret_cast<uint8_t *>(prefix), sizeof(NetworkDataTlv) + prefix->GetLength());
                continue;
            }

            dump("remove prefix done", m_tlvs, m_length);
            break;
        }

        default:
        {
            assert(false);
            break;
        }
        }

        cur = cur->GetNext();
    }

    dump("remove done", m_tlvs, m_length);

    return kThreadError_None;
}

ThreadError Leader::RemoveRloc(PrefixTlv &prefix, uint16_t rloc)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end;
    ContextTlv *context;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

        if (cur >= end)
        {
            break;
        }

        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypeHasRoute:
            RemoveRloc(prefix, *reinterpret_cast<HasRouteTlv *>(cur), rloc);

            // remove has route tlv if empty
            if (cur->GetLength() == 0)
            {
                prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - sizeof(HasRouteTlv));
                Remove(reinterpret_cast<uint8_t *>(cur), sizeof(HasRouteTlv));
                continue;
            }

            break;

        case NetworkDataTlv::kTypeBorderRouter:
            RemoveRloc(prefix, *reinterpret_cast<BorderRouterTlv *>(cur), rloc);

            // remove border router tlv if empty
            if (cur->GetLength() == 0)
            {
                prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - sizeof(BorderRouterTlv));
                Remove(reinterpret_cast<uint8_t *>(cur), sizeof(BorderRouterTlv));
                continue;
            }

            break;

        case NetworkDataTlv::kTypeContext:
            break;

        default:
            assert(false);
            break;
        }

        cur = cur->GetNext();
    }

    if ((context = FindContext(prefix)) != NULL)
    {
        if (prefix.GetSubTlvsLength() == sizeof(ContextTlv))
        {
            context->ClearCompress();
            m_context_last_used[context->GetContextId() - kMinContextId] = Timer::GetNow();

            if (m_context_last_used[context->GetContextId() - kMinContextId] == 0)
            {
                m_context_last_used[context->GetContextId() - kMinContextId] = 1;
            }

            m_timer.Start(1000);
        }
        else
        {
            context->SetCompress();
            m_context_last_used[context->GetContextId() - kMinContextId] = 0;
        }
    }

    return kThreadError_None;
}

ThreadError Leader::RemoveRloc(PrefixTlv &prefix, HasRouteTlv &has_route, uint16_t rloc)
{
    HasRouteEntry *entry;

    // remove rloc from has route tlv
    for (int i = 0; i < has_route.GetNumEntries(); i++)
    {
        entry = has_route.GetEntry(i);

        if (entry->GetRloc() != rloc)
        {
            continue;
        }

        has_route.SetLength(has_route.GetLength() - sizeof(HasRouteEntry));
        prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - sizeof(HasRouteEntry));
        Remove(reinterpret_cast<uint8_t *>(entry), sizeof(*entry));
        break;
    }

    return kThreadError_None;
}

ThreadError Leader::RemoveRloc(PrefixTlv &prefix, BorderRouterTlv &border_router, uint16_t rloc)
{
    BorderRouterEntry *entry;

    // remove rloc from border router tlv
    for (int i = 0; i < border_router.GetNumEntries(); i++)
    {
        entry = border_router.GetEntry(i);

        if (entry->GetRloc() != rloc)
        {
            continue;
        }

        border_router.SetLength(border_router.GetLength() - sizeof(BorderRouterEntry));
        prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - sizeof(BorderRouterEntry));
        Remove(reinterpret_cast<uint8_t *>(entry), sizeof(*entry));
        break;
    }

    return kThreadError_None;
}

ThreadError Leader::RemoveContext(uint8_t context_id)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
    NetworkDataTlv *end;
    PrefixTlv *prefix;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);

        if (cur >= end)
        {
            break;
        }

        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
        {
            prefix = reinterpret_cast<PrefixTlv *>(cur);
            RemoveContext(*prefix, context_id);

            if (prefix->GetSubTlvsLength() == 0)
            {
                Remove(reinterpret_cast<uint8_t *>(prefix), sizeof(NetworkDataTlv) + prefix->GetLength());
                continue;
            }

            dump("remove prefix done", m_tlvs, m_length);
            break;
        }

        default:
        {
            assert(false);
            break;
        }
        }

        cur = cur->GetNext();
    }

    dump("remove done", m_tlvs, m_length);

    return kThreadError_None;
}

ThreadError Leader::RemoveContext(PrefixTlv &prefix, uint8_t context_id)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end;
    ContextTlv *context;
    uint8_t length;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

        if (cur >= end)
        {
            break;
        }

        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypeBorderRouter:
        {
            break;
        }

        case NetworkDataTlv::kTypeContext:
        {
            // remove context tlv
            context = reinterpret_cast<ContextTlv *>(cur);

            if (context->GetContextId() == context_id)
            {
                length = sizeof(NetworkDataTlv) + context->GetLength();
                prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - length);
                Remove(reinterpret_cast<uint8_t *>(context), length);
                continue;
            }

            break;
        }

        default:
        {
            assert(false);
            break;
        }
        }

        cur = cur->GetNext();
    }

    return kThreadError_None;
}

void Leader::HandleTimer(void *context)
{
    Leader *obj = reinterpret_cast<Leader *>(context);
    obj->HandleTimer();
}

void Leader::HandleTimer()
{
    bool contexts_waiting = false;

    for (int i = 0; i < kNumContextIds; i++)
    {
        if (m_context_last_used[i] == 0)
        {
            continue;
        }

        if ((Timer::GetNow() - m_context_last_used[i]) >= m_context_id_reuse_delay * 1000U)
        {
            FreeContext(kMinContextId + i);
        }
        else
        {
            contexts_waiting = true;
        }
    }

    if (contexts_waiting)
    {
        m_timer.Start(1000);
    }
}

}  // namespace NetworkData
}  // namespace Thread
