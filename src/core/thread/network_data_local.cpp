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
 *   This file implements the local Thread Network Data.
 */

#include <coap/coap_header.hpp>
#include <common/code_utils.hpp>
#include <platform/random.h>
#include <thread/network_data_local.hpp>
#include <thread/thread_netif.hpp>
#include <thread/thread_tlvs.hpp>

namespace Thread {
namespace NetworkData {

Local::Local(ThreadNetif &netif)
{
    mMle = netif.GetMle();
}

ThreadError Local::AddOnMeshPrefix(const uint8_t *prefix, uint8_t prefixLength, int8_t prf,
                                   uint8_t flags, bool stable)
{
    PrefixTlv *prefixTlv;
    BorderRouterTlv *brTlv;

    RemoveOnMeshPrefix(prefix, prefixLength);

    prefixTlv = reinterpret_cast<PrefixTlv *>(mTlvs + mLength);
    Insert(reinterpret_cast<uint8_t *>(prefixTlv),
           sizeof(PrefixTlv) + (prefixLength + 7) / 8 + sizeof(BorderRouterTlv) + sizeof(BorderRouterEntry));
    prefixTlv->Init(0, prefixLength, prefix);
    prefixTlv->SetSubTlvsLength(sizeof(BorderRouterTlv) + sizeof(BorderRouterEntry));

    brTlv = reinterpret_cast<BorderRouterTlv *>(prefixTlv->GetSubTlvs());
    brTlv->Init();
    brTlv->SetLength(brTlv->GetLength() + sizeof(BorderRouterEntry));
    brTlv->GetEntry(0)->Init();
    brTlv->GetEntry(0)->SetPreference(prf);
    brTlv->GetEntry(0)->SetFlags(flags);

    if (stable)
    {
        prefixTlv->SetStable();
        brTlv->SetStable();
    }

    dump("add prefix done", mTlvs, mLength);
    return kThreadError_None;
}

ThreadError Local::RemoveOnMeshPrefix(const uint8_t *prefix, uint8_t prefixLength)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *tlv;

    VerifyOrExit((tlv = FindPrefix(prefix, prefixLength)) != NULL, error = kThreadError_Error);
    VerifyOrExit(FindBorderRouter(*tlv) != NULL, error = kThreadError_Error);
    Remove(reinterpret_cast<uint8_t *>(tlv), sizeof(NetworkDataTlv) + tlv->GetLength());

exit:
    dump("remove done", mTlvs, mLength);
    return error;
}

ThreadError Local::AddHasRoutePrefix(const uint8_t *prefix, uint8_t prefixLength, int8_t prf, bool stable)
{
    PrefixTlv *prefixTlv;
    HasRouteTlv *hasRouteTlv;

    RemoveHasRoutePrefix(prefix, prefixLength);

    prefixTlv = reinterpret_cast<PrefixTlv *>(mTlvs + mLength);
    Insert(reinterpret_cast<uint8_t *>(prefixTlv),
           sizeof(PrefixTlv) + (prefixLength + 7) / 8 + sizeof(HasRouteTlv) + sizeof(HasRouteEntry));
    prefixTlv->Init(0, prefixLength, prefix);
    prefixTlv->SetSubTlvsLength(sizeof(HasRouteTlv) + sizeof(HasRouteEntry));

    hasRouteTlv = reinterpret_cast<HasRouteTlv *>(prefixTlv->GetSubTlvs());
    hasRouteTlv->Init();
    hasRouteTlv->SetLength(hasRouteTlv->GetLength() + sizeof(HasRouteEntry));
    hasRouteTlv->GetEntry(0)->Init();
    hasRouteTlv->GetEntry(0)->SetPreference(prf);

    if (stable)
    {
        prefixTlv->SetStable();
        hasRouteTlv->SetStable();
    }

    dump("add route done", mTlvs, mLength);
    return kThreadError_None;
}

ThreadError Local::RemoveHasRoutePrefix(const uint8_t *prefix, uint8_t prefixLength)
{
    ThreadError error = kThreadError_None;
    PrefixTlv *tlv;

    VerifyOrExit((tlv = FindPrefix(prefix, prefixLength)) != NULL, error = kThreadError_Error);
    VerifyOrExit(FindHasRoute(*tlv) != NULL, error = kThreadError_Error);
    Remove(reinterpret_cast<uint8_t *>(tlv), sizeof(NetworkDataTlv) + tlv->GetLength());

exit:
    dump("remove done", mTlvs, mLength);
    return error;
}

ThreadError Local::UpdateRloc()
{
    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
         cur < reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
         cur = cur->GetNext())
    {
        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
            UpdateRloc(*reinterpret_cast<PrefixTlv *>(cur));
            break;

        default:
            assert(false);
            break;
        }
    }

    return kThreadError_None;
}

ThreadError Local::UpdateRloc(PrefixTlv &prefix)
{
    for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
         cur < reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());
         cur = cur->GetNext())
    {
        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypeHasRoute:
            UpdateRloc(*reinterpret_cast<HasRouteTlv *>(cur));
            break;

        case NetworkDataTlv::kTypeBorderRouter:
            UpdateRloc(*reinterpret_cast<BorderRouterTlv *>(cur));
            break;

        default:
            assert(false);
            break;
        }
    }

    return kThreadError_None;
}

ThreadError Local::UpdateRloc(HasRouteTlv &hasRoute)
{
    HasRouteEntry *entry = hasRoute.GetEntry(0);
    entry->SetRloc(mMle->GetRloc16());
    return kThreadError_None;
}

ThreadError Local::UpdateRloc(BorderRouterTlv &border_router)
{
    BorderRouterEntry *entry = border_router.GetEntry(0);
    entry->SetRloc(mMle->GetRloc16());
    return kThreadError_None;
}

ThreadError Local::Register(const Ip6Address &destination)
{
    ThreadError error = kThreadError_None;
    Coap::Header header;
    Message *message;
    Ip6MessageInfo messageInfo;

    UpdateRloc();
    mSocket.Open(&HandleUdpReceive, this);

    for (size_t i = 0; i < sizeof(mCoapToken); i++)
    {
        mCoapToken[i] = ot_random_get();
    }

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(mCoapToken, sizeof(mCoapToken));
    header.AppendUriPathOptions("n/sd");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));
    SuccessOrExit(error = message->Append(mTlvs, mLength));

    memset(&messageInfo, 0, sizeof(messageInfo));
    memcpy(&messageInfo.mPeerAddr, &destination, sizeof(messageInfo.mPeerAddr));
    messageInfo.mPeerPort = kCoapUdpPort;
    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    dprintf("Sent network data registration\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void Local::HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo)
{
    Local *obj = reinterpret_cast<Local *>(context);
    obj->HandleUdpReceive(*static_cast<Message *>(message), *static_cast<const Ip6MessageInfo *>(messageInfo));
}

void Local::HandleUdpReceive(Message &message, const Ip6MessageInfo &messageInfo)
{
    Coap::Header header;

    SuccessOrExit(header.FromMessage(message));
    VerifyOrExit(header.GetType() == Coap::Header::kTypeAcknowledgment &&
                 header.GetCode() == Coap::Header::kCodeChanged &&
                 header.GetMessageId() == mCoapMessageId &&
                 header.GetTokenLength() == sizeof(mCoapToken) &&
                 memcmp(mCoapToken, header.GetToken(), sizeof(mCoapToken)) == 0, ;);

    dprintf("Network data registration acknowledged\n");

exit:
    {}
}

}  // namespace NetworkData
}  // namespace Thread

