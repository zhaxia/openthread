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

#include <coap/coap_header.hpp>
#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/message.hpp>
#include <common/random.hpp>
#include <common/thread_error.hpp>
#include <common/timer.hpp>
#include <mac/mac_frame.hpp>
#include <thread/mle_router.hpp>
#include <thread/network_data_leader.hpp>
#include <thread/thread_netif.hpp>
#include <thread/thread_tlvs.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace NetworkData {

Leader::Leader(ThreadNetif &netif):
    mServerData("n/sd", &HandleServerData, this),
    mTimer(&HandleTimer, this)
{
    mCoapServer = netif.GetCoapServer();
    mNetif = &netif;
    mMle = netif.GetMle();
}

ThreadError Leader::Init()
{
    memset(mAddresses, 0, sizeof(mAddresses));
    memset(mContextLastUsed, 0, sizeof(mContextLastUsed));
    mVersion = Random::Get();
    mStableVersion = Random::Get();
    mLength = 0;
    return kThreadError_None;
}

ThreadError Leader::Start()
{
    return mCoapServer->AddResource(mServerData);
}

ThreadError Leader::Stop()
{
    return kThreadError_None;
}

uint8_t Leader::GetVersion() const
{
    return mVersion;
}

uint8_t Leader::GetStableVersion() const
{
    return mStableVersion;
}

uint32_t Leader::GetContextIdReuseDelay() const
{
    return mContextIdReuseDelay;
}

ThreadError Leader::SetContextIdReuseDelay(uint32_t delay)
{
    mContextIdReuseDelay = delay;
    return kThreadError_None;
}

ThreadError Leader::GetContext(const Ip6Address &address, Context &context)
{
    PrefixTlv *prefix;
    ContextTlv *contextTlv;

    context.mPrefixLength = 0;

    if (PrefixMatch(mMle->GetMeshLocalPrefix(), address.mAddr8, 64) >= 0)
    {
        context.mPrefix = mMle->GetMeshLocalPrefix();
        context.mPrefixLength = 64;
        context.mContextId = 0;
    }

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);

        if (PrefixMatch(prefix->GetPrefix(), address.mAddr8, prefix->GetPrefixLength()) < 0)
        {
            continue;
        }

        contextTlv = FindContext(*prefix);

        if (contextTlv == NULL)
        {
            continue;
        }

        if (prefix->GetPrefixLength() > context.mPrefixLength)
        {
            context.mPrefix = prefix->GetPrefix();
            context.mPrefixLength = prefix->GetPrefixLength();
            context.mContextId = contextTlv->GetContextId();
        }
    }

    return (context.mPrefixLength > 0) ? kThreadError_None : kThreadError_Error;
}

ThreadError Leader::GetContext(uint8_t context_id, Context &context)
{
    ThreadError error = kThreadError_Error;
    PrefixTlv *prefix;
    ContextTlv *contextTlv;

    if (context_id == 0)
    {
        context.mPrefix = mMle->GetMeshLocalPrefix();
        context.mPrefixLength = 64;
        context.mContextId = 0;
        ExitNow(error = kThreadError_None);
    }

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);
        contextTlv = FindContext(*prefix);

        if (contextTlv == NULL)
        {
            continue;
        }

        if (contextTlv->GetContextId() != context_id)
        {
            continue;
        }

        context.mPrefix = prefix->GetPrefix();
        context.mPrefixLength = prefix->GetPrefixLength();
        context.mContextId = contextTlv->GetContextId();
        ExitNow(error = kThreadError_None);
    }

exit:
    return error;
}

ThreadError Leader::ConfigureAddresses()
{
    PrefixTlv *prefix;

    // clear out addresses that are not on-mesh
    for (size_t i = 0; i < sizeof(mAddresses) / sizeof(mAddresses[0]); i++)
    {
        if (mAddresses[i].mValidLifetime == 0 ||
            IsOnMesh(mAddresses[i].mAddress))
        {
            continue;
        }

        mNetif->RemoveUnicastAddress(mAddresses[i]);
        mAddresses[i].mValidLifetime = 0;
    }

    // configure on-mesh addresses
    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
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
    BorderRouterTlv *borderRouter;
    BorderRouterEntry *entry;

    // look for Border Router TLV
    if ((borderRouter = FindBorderRouter(prefix)) == NULL)
    {
        ExitNow();
    }

    // check if Valid flag is set
    if ((entry = borderRouter->GetEntry(0)) == NULL ||
        entry->IsValid() == false)
    {
        ExitNow();
    }

    // check if address is already added for this prefix
    for (size_t i = 0; i < sizeof(mAddresses) / sizeof(mAddresses[0]); i++)
    {
        if (mAddresses[i].mValidLifetime != 0 &&
            mAddresses[i].mPrefixLength == prefix.GetPrefixLength() &&
            PrefixMatch(mAddresses[i].mAddress.mAddr8, prefix.GetPrefix(), prefix.GetPrefixLength()) >= 0)
        {
            mAddresses[i].mPreferredLifetime = entry->IsPreferred() ? 0xffffffff : 0;
            ExitNow();
        }
    }

    // configure address for this prefix
    for (size_t i = 0; i < sizeof(mAddresses) / sizeof(mAddresses[0]); i++)
    {
        if (mAddresses[i].mValidLifetime != 0)
        {
            continue;
        }

        memset(&mAddresses[i], 0, sizeof(mAddresses[i]));
        memcpy(mAddresses[i].mAddress.mAddr8, prefix.GetPrefix(), (prefix.GetPrefixLength() + 7) / 8);

        for (size_t j = 8; j < sizeof(mAddresses[i].mAddress); j++)
        {
            mAddresses[i].mAddress.mAddr8[j] = Random::Get();
        }

        mAddresses[i].mPrefixLength = prefix.GetPrefixLength();
        mAddresses[i].mPreferredLifetime = entry->IsPreferred() ? 0xffffffff : 0;
        mAddresses[i].mValidLifetime = 0xffffffff;
        mNetif->AddUnicastAddress(mAddresses[i]);
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

    if (memcmp(address.mAddr8, mMle->GetMeshLocalPrefix(), 8) == 0)
    {
        ExitNow(rval = true);
    }

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);

        if (PrefixMatch(prefix->GetPrefix(), address.mAddr8, prefix->GetPrefixLength()) < 0)
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

    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
         cur = cur->GetNext())
    {
        if (cur->GetType() != NetworkDataTlv::kTypePrefix)
        {
            continue;
        }

        prefix = reinterpret_cast<PrefixTlv *>(cur);

        if (PrefixMatch(prefix->GetPrefix(), source.mAddr8, prefix->GetPrefixLength()) >= 0)
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
    HasRouteTlv *hasRoute;
    HasRouteEntry *entry;
    HasRouteEntry *rvalRoute = NULL;
    int8_t rval_plen = 0;
    int8_t plen;
    NetworkDataTlv *cur;

    for (cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
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

        plen = PrefixMatch(prefix->GetPrefix(), destination.mAddr8, prefix->GetPrefixLength());

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

                hasRoute = reinterpret_cast<HasRouteTlv *>(cur);

                for (int i = 0; i < hasRoute->GetNumEntries(); i++)
                {
                    entry = hasRoute->GetEntry(i);

                    if (rvalRoute == NULL ||
                        entry->GetPreference() > rvalRoute->GetPreference() ||
                        (entry->GetPreference() == rvalRoute->GetPreference() &&
                         mMle->GetRouteCost(entry->GetRloc()) < mMle->GetRouteCost(rvalRoute->GetRloc())))
                    {
                        rvalRoute = entry;
                        rval_plen = plen;
                    }
                }

            }
        }
    }

    if (rvalRoute != NULL)
    {
        if (rloc != NULL)
        {
            *rloc = rvalRoute->GetRloc();
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
    BorderRouterTlv *borderRouter;
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

        borderRouter = reinterpret_cast<BorderRouterTlv *>(cur);

        for (int i = 0; i < borderRouter->GetNumEntries(); i++)
        {
            entry = borderRouter->GetEntry(i);

            if (entry->IsDefaultRoute() == false)
            {
                continue;
            }

            if (route == NULL ||
                entry->GetPreference() > route->GetPreference() ||
                (entry->GetPreference() == route->GetPreference() &&
                 mMle->GetRouteCost(entry->GetRloc()) < mMle->GetRouteCost(route->GetRloc())))
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
                                   const uint8_t *data, uint8_t dataLength)
{
    mVersion = version;
    mStableVersion = stable_version;
    memcpy(mTlvs, data, dataLength);
    mLength = dataLength;

    if (stable)
    {
        RemoveTemporaryData(mTlvs, mLength);
    }

    dump("set network data", mTlvs, mLength);

    ConfigureAddresses();
    mMle->HandleNetworkDataUpdate();

    return kThreadError_None;
}

ThreadError Leader::RemoveBorderRouter(uint16_t rloc)
{
    RemoveRloc(rloc);
    ConfigureAddresses();
    mMle->HandleNetworkDataUpdate();
    return kThreadError_None;
}

void Leader::HandleServerData(void *context, Coap::Header &header, Message &message,
                              const Ip6MessageInfo &messageInfo)
{
    Leader *obj = reinterpret_cast<Leader *>(context);
    obj->HandleServerData(header, message, messageInfo);
}

void Leader::HandleServerData(Coap::Header &header, Message &message,
                              const Ip6MessageInfo &messageInfo)
{
    uint8_t tlvsLength;
    uint8_t tlvs[256];
    uint16_t rloc16;

    dprintf("Received network data registration\n");

    tlvsLength = message.GetLength() - message.GetOffset();

    message.Read(message.GetOffset(), tlvsLength, tlvs);
    rloc16 = HostSwap16(messageInfo.mPeerAddr.mAddr16[7]);
    RegisterNetworkData(rloc16, tlvs, tlvsLength);

    SendServerDataResponse(header, messageInfo, tlvs, tlvsLength);
}

void Leader::SendServerDataResponse(const Coap::Header &requestHeader, const Ip6MessageInfo &messageInfo,
                                    const uint8_t *tlvs, uint8_t tlvsLength)
{
    ThreadError error = kThreadError_None;
    Coap::Header responseHeader;
    Message *message;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    responseHeader.Init();
    responseHeader.SetVersion(1);
    responseHeader.SetType(Coap::Header::kTypeAcknowledgment);
    responseHeader.SetCode(Coap::Header::kCodeChanged);
    responseHeader.SetMessageId(requestHeader.GetMessageId());
    responseHeader.SetToken(requestHeader.GetToken(), requestHeader.GetTokenLength());
    responseHeader.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    responseHeader.Finalize();
    SuccessOrExit(error = message->Append(responseHeader.GetBytes(), responseHeader.GetLength()));
    SuccessOrExit(error = message->Append(tlvs, tlvsLength));

    SuccessOrExit(error = mCoapServer->SendMessage(*message, messageInfo));

    dprintf("Sent network data registration acknowledgment\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError Leader::RegisterNetworkData(uint16_t rloc, uint8_t *tlvs, uint8_t tlvsLength)
{
    ThreadError error = kThreadError_None;

    SuccessOrExit(error = RemoveRloc(rloc));
    SuccessOrExit(error = AddNetworkData(tlvs, tlvsLength));

    mVersion++;
    mStableVersion++;

    ConfigureAddresses();
    mMle->HandleNetworkDataUpdate();

exit:
    return error;
}

ThreadError Leader::AddNetworkData(uint8_t *tlvs, uint8_t tlvsLength)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(tlvs);
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(tlvs + tlvsLength);

    while (cur < end)
    {
        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
            AddPrefix(*reinterpret_cast<PrefixTlv *>(cur));
            dump("add prefix done", mTlvs, mLength);
            break;

        default:
            assert(false);
            break;
        }

        cur = cur->GetNext();
    }

    dump("add done", mTlvs, mLength);

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

ThreadError Leader::AddHasRoute(PrefixTlv &prefix, HasRouteTlv &hasRoute)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *dstPrefix;
    HasRouteTlv *dstHasRoute;

    if ((dstPrefix = FindPrefix(prefix.GetPrefix(), prefix.GetPrefixLength())) == NULL)
    {
        dstPrefix = reinterpret_cast<PrefixTlv *>(mTlvs + mLength);
        Insert(reinterpret_cast<uint8_t *>(dstPrefix), sizeof(PrefixTlv) + (prefix.GetPrefixLength() + 7) / 8);
        dstPrefix->Init(prefix.GetDomainId(), prefix.GetPrefixLength(), prefix.GetPrefix());
    }

    if (hasRoute.IsStable())
    {
        dstPrefix->SetStable();
    }

    if ((dstHasRoute = FindHasRoute(*dstPrefix, hasRoute.IsStable())) == NULL)
    {
        dstHasRoute = reinterpret_cast<HasRouteTlv *>(dstPrefix->GetNext());
        Insert(reinterpret_cast<uint8_t *>(dstHasRoute), sizeof(HasRouteTlv));
        dstPrefix->SetLength(dstPrefix->GetLength() + sizeof(HasRouteTlv));
        dstHasRoute->Init();

        if (hasRoute.IsStable())
        {
            dstHasRoute->SetStable();
        }
    }

    Insert(reinterpret_cast<uint8_t *>(dstHasRoute->GetNext()), sizeof(HasRouteEntry));
    dstHasRoute->SetLength(dstHasRoute->GetLength() + sizeof(HasRouteEntry));
    dstPrefix->SetLength(dstPrefix->GetLength() + sizeof(HasRouteEntry));
    memcpy(dstHasRoute->GetEntry(dstHasRoute->GetNumEntries() - 1), hasRoute.GetEntry(0),
           sizeof(HasRouteEntry));

    return error;
}

ThreadError Leader::AddBorderRouter(PrefixTlv &prefix, BorderRouterTlv &borderRouter)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *dstPrefix;
    ContextTlv *dst_context;
    BorderRouterTlv *dst_borderRouter;
    uint8_t context_id;

    if ((dstPrefix = FindPrefix(prefix.GetPrefix(), prefix.GetPrefixLength())) == NULL)
    {
        dstPrefix = reinterpret_cast<PrefixTlv *>(mTlvs + mLength);
        Insert(reinterpret_cast<uint8_t *>(dstPrefix), sizeof(PrefixTlv) + (prefix.GetPrefixLength() + 7) / 8);
        dstPrefix->Init(prefix.GetDomainId(), prefix.GetPrefixLength(), prefix.GetPrefix());
    }

    if (borderRouter.IsStable())
    {
        dstPrefix->SetStable();

        if ((dst_context = FindContext(*dstPrefix)) != NULL)
        {
            dst_context->SetCompress();
        }
        else if ((context_id = AllocateContext()) >= 0)
        {
            dst_context = reinterpret_cast<ContextTlv *>(dstPrefix->GetNext());
            Insert(reinterpret_cast<uint8_t *>(dst_context), sizeof(ContextTlv));
            dstPrefix->SetLength(dstPrefix->GetLength() + sizeof(ContextTlv));
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

        mContextLastUsed[dst_context->GetContextId() - kMinContextId] = 0;
    }

    if ((dst_borderRouter = FindBorderRouter(*dstPrefix, borderRouter.IsStable())) == NULL)
    {
        dst_borderRouter = reinterpret_cast<BorderRouterTlv *>(dstPrefix->GetNext());
        Insert(reinterpret_cast<uint8_t *>(dst_borderRouter), sizeof(BorderRouterTlv));
        dstPrefix->SetLength(dstPrefix->GetLength() + sizeof(BorderRouterTlv));
        dst_borderRouter->Init();

        if (borderRouter.IsStable())
        {
            dst_borderRouter->SetStable();
        }
    }

    Insert(reinterpret_cast<uint8_t *>(dst_borderRouter->GetNext()), sizeof(BorderRouterEntry));
    dst_borderRouter->SetLength(dst_borderRouter->GetLength() + sizeof(BorderRouterEntry));
    dstPrefix->SetLength(dstPrefix->GetLength() + sizeof(BorderRouterEntry));
    memcpy(dst_borderRouter->GetEntry(dst_borderRouter->GetNumEntries() - 1), borderRouter.GetEntry(0),
           sizeof(BorderRouterEntry));

exit:
    return error;
}

int Leader::AllocateContext()
{
    int rval = -1;

    for (int i = kMinContextId; i < kMinContextId + kNumContextIds; i++)
    {
        if ((mContextUsed & (1 << i)) == 0)
        {
            mContextUsed |= 1 << i;
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
    mContextUsed &= ~(1 << context_id);
    mVersion++;
    mStableVersion++;
    mMle->HandleNetworkDataUpdate();
    return kThreadError_None;
}

ThreadError Leader::RemoveRloc(uint16_t rloc)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
    NetworkDataTlv *end;
    PrefixTlv *prefix;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);

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

            dump("remove prefix done", mTlvs, mLength);
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

    dump("remove done", mTlvs, mLength);

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
            mContextLastUsed[context->GetContextId() - kMinContextId] = Timer::GetNow();

            if (mContextLastUsed[context->GetContextId() - kMinContextId] == 0)
            {
                mContextLastUsed[context->GetContextId() - kMinContextId] = 1;
            }

            mTimer.Start(1000);
        }
        else
        {
            context->SetCompress();
            mContextLastUsed[context->GetContextId() - kMinContextId] = 0;
        }
    }

    return kThreadError_None;
}

ThreadError Leader::RemoveRloc(PrefixTlv &prefix, HasRouteTlv &hasRoute, uint16_t rloc)
{
    HasRouteEntry *entry;

    // remove rloc from has route tlv
    for (int i = 0; i < hasRoute.GetNumEntries(); i++)
    {
        entry = hasRoute.GetEntry(i);

        if (entry->GetRloc() != rloc)
        {
            continue;
        }

        hasRoute.SetLength(hasRoute.GetLength() - sizeof(HasRouteEntry));
        prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - sizeof(HasRouteEntry));
        Remove(reinterpret_cast<uint8_t *>(entry), sizeof(*entry));
        break;
    }

    return kThreadError_None;
}

ThreadError Leader::RemoveRloc(PrefixTlv &prefix, BorderRouterTlv &borderRouter, uint16_t rloc)
{
    BorderRouterEntry *entry;

    // remove rloc from border router tlv
    for (int i = 0; i < borderRouter.GetNumEntries(); i++)
    {
        entry = borderRouter.GetEntry(i);

        if (entry->GetRloc() != rloc)
        {
            continue;
        }

        borderRouter.SetLength(borderRouter.GetLength() - sizeof(BorderRouterEntry));
        prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - sizeof(BorderRouterEntry));
        Remove(reinterpret_cast<uint8_t *>(entry), sizeof(*entry));
        break;
    }

    return kThreadError_None;
}

ThreadError Leader::RemoveContext(uint8_t context_id)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
    NetworkDataTlv *end;
    PrefixTlv *prefix;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);

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

            dump("remove prefix done", mTlvs, mLength);
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

    dump("remove done", mTlvs, mLength);

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
        if (mContextLastUsed[i] == 0)
        {
            continue;
        }

        if ((Timer::GetNow() - mContextLastUsed[i]) >= mContextIdReuseDelay * 1000U)
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
        mTimer.Start(1000);
    }
}

}  // namespace NetworkData
}  // namespace Thread
