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
 *   This file implements Thread's EID-to-RLOC mapping and caching.
 */

#include <coap/coap_header.hpp>
#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <common/thread_error.hpp>
#include <mac/mac_frame.hpp>
#include <platform/random.h>
#include <thread/address_resolver.hpp>
#include <thread/mesh_forwarder.hpp>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>
#include <thread/thread_tlvs.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

AddressResolver::AddressResolver(ThreadNetif &netif) :
    mAddressError("a/ae", &HandleAddressError, this),
    mAddressQuery("a/aq", &HandleAddressQuery, this),
    mAddressNotification("a/an", &HandleAddressNotification, this),
    mIcmp6Handler(&HandleDstUnreach, this),
    mTimer(&HandleTimer, this)
{
    memset(&mCache, 0, sizeof(mCache));
    mMeshForwarder = netif.GetMeshForwarder();
    mMle = netif.GetMle();
    mNetif = &netif;

    mCoapServer = netif.GetCoapServer();
    mCoapServer->AddResource(mAddressError);
    mCoapServer->AddResource(mAddressQuery);
    mCoapServer->AddResource(mAddressNotification);
    mCoapMessageId = otRandomGet();

    Icmp6::RegisterCallbacks(mIcmp6Handler);
}

ThreadError AddressResolver::Clear()
{
    memset(&mCache, 0, sizeof(mCache));
    return kThreadError_None;
}

ThreadError AddressResolver::Remove(uint8_t routerId)
{
    for (int i = 0; i < kCacheEntries; i++)
    {
        if ((mCache[i].mRloc >> 10) == routerId)
        {
            mCache[i].mState = Cache::kStateInvalid;
        }
    }

    return kThreadError_None;
}

ThreadError AddressResolver::Resolve(const Ip6Address &dst, Mac::Address16 &rloc)
{
    ThreadError error = kThreadError_None;
    Cache *entry = NULL;

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (mCache[i].mState != Cache::kStateInvalid)
        {
            if (memcmp(&mCache[i].mTarget, &dst, sizeof(mCache[i].mTarget)) == 0)
            {
                entry = &mCache[i];
                break;
            }
        }
        else if (entry == NULL)
        {
            entry = &mCache[i];
        }
    }

    VerifyOrExit(entry != NULL, error = kThreadError_NoBufs);

    switch (entry->mState)
    {
    case Cache::kStateInvalid:
        memcpy(&entry->mTarget, &dst, sizeof(entry->mTarget));
        entry->mState = Cache::kStateDiscover;
        entry->mTimeout = kDiscoverTimeout;
        mTimer.Start(1000);
        SendAddressQuery(dst);
        error = kThreadError_LeaseQuery;
        break;

    case Cache::kStateDiscover:
    case Cache::kStateRetry:
        error = kThreadError_LeaseQuery;
        break;

    case Cache::kStateValid:
        rloc = entry->mRloc;
        break;
    }

exit:
    return error;
}

ThreadError AddressResolver::SendAddressQuery(const Ip6Address &eid)
{
    ThreadError error;
    SockAddr sockaddr = {};
    Message *message;
    Coap::Header header;
    ThreadTargetTlv targetTlv;
    Ip6MessageInfo messageInfo;

    sockaddr.mPort = kCoapUdpPort;
    mSocket.Open(&HandleUdpReceive, this);
    mSocket.Bind(sockaddr);

    for (size_t i = 0; i < sizeof(mCoapToken); i++)
    {
        mCoapToken[i] = otRandomGet();
    }

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeNonConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions("a/aq");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    targetTlv.Init();
    targetTlv.SetTarget(eid);
    SuccessOrExit(error = message->Append(&targetTlv, sizeof(targetTlv)));

    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.GetPeerAddr().m16[0] = HostSwap16(0xff03);
    messageInfo.GetPeerAddr().m16[7] = HostSwap16(0x0002);
    messageInfo.mPeerPort = kCoapUdpPort;
    messageInfo.mInterfaceId = mNetif->GetInterfaceId();

    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    dprintf("Sent address query\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void AddressResolver::HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo)
{
}

void AddressResolver::HandleAddressNotification(void *context, Coap::Header &header, Message &message,
                                                const Ip6MessageInfo &messageInfo)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleAddressNotification(header, message, messageInfo);
}

void AddressResolver::HandleAddressNotification(Coap::Header &header, Message &message,
                                                const Ip6MessageInfo &messageInfo)
{
    ThreadTargetTlv targetTlv;
    ThreadMeshLocalIidTlv mlIidTlv;
    ThreadRlocTlv rlocTlv;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address notification from %04x\n", HostSwap16(messageInfo.GetPeerAddr().m16[7]));

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kTarget, sizeof(targetTlv), targetTlv));
    VerifyOrExit(targetTlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kMeshLocalIid, sizeof(mlIidTlv), mlIidTlv));
    VerifyOrExit(mlIidTlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rlocTlv), rlocTlv));
    VerifyOrExit(rlocTlv.IsValid(), ;);

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (memcmp(&mCache[i].mTarget, targetTlv.GetTarget(), sizeof(mCache[i].mTarget)) == 0)
        {
            if (mCache[i].mState != Cache::kStateValid ||
                memcmp(mCache[i].mIid, mlIidTlv.GetIid(), sizeof(mCache[i].mIid)) == 0)
            {
                memcpy(mCache[i].mIid, mlIidTlv.GetIid(), sizeof(mCache[i].mIid));
                mCache[i].mRloc = rlocTlv.GetRloc16();
                mCache[i].mTimeout = 0;
                mCache[i].mFailureCount = 0;
                mCache[i].mState = Cache::kStateValid;
                SendAddressNotificationResponse(header, messageInfo);
                mMeshForwarder->HandleResolved(*targetTlv.GetTarget());
            }
            else
            {
                SendAddressError(targetTlv, mlIidTlv, NULL);
            }

            ExitNow();
        }
    }

    ExitNow();

exit:
    {}
}

void AddressResolver::SendAddressNotificationResponse(const Coap::Header &requestHeader,
                                                      const Ip6MessageInfo &requestInfo)
{
    ThreadError error;
    Message *message;
    Coap::Header responseHeader;
    Ip6MessageInfo responseInfo;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    responseHeader.Init();
    responseHeader.SetVersion(1);
    responseHeader.SetType(Coap::Header::kTypeAcknowledgment);
    responseHeader.SetCode(Coap::Header::kCodeChanged);
    responseHeader.SetMessageId(requestHeader.GetMessageId());
    responseHeader.SetToken(requestHeader.GetToken(), requestHeader.GetTokenLength());
    responseHeader.Finalize();
    SuccessOrExit(error = message->Append(responseHeader.GetBytes(), responseHeader.GetLength()));

    memcpy(&responseInfo, &requestInfo, sizeof(responseInfo));
    memset(&responseInfo.mSockAddr, 0, sizeof(responseInfo.mSockAddr));
    SuccessOrExit(error = mCoapServer->SendMessage(*message, responseInfo));

    dprintf("Sent address notification acknowledgment\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError AddressResolver::SendAddressError(const ThreadTargetTlv &target, const ThreadMeshLocalIidTlv &eid,
                                              const Ip6Address *destination)
{
    ThreadError error;
    Message *message;
    Coap::Header header;
    Ip6MessageInfo messageInfo;
    SockAddr sockaddr = {};

    sockaddr.mPort = kCoapUdpPort;
    mSocket.Open(&HandleUdpReceive, this);
    mSocket.Bind(sockaddr);

    for (size_t i = 0; i < sizeof(mCoapToken); i++)
    {
        mCoapToken[i] = otRandomGet();
    }

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeNonConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions("a/ae");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));
    SuccessOrExit(error = message->Append(&target, sizeof(target)));
    SuccessOrExit(error = message->Append(&eid, sizeof(eid)));

    memset(&messageInfo, 0, sizeof(messageInfo));

    if (destination == NULL)
    {
        messageInfo.GetPeerAddr().m16[0] = HostSwap16(0xff03);
        messageInfo.GetPeerAddr().m16[7] = HostSwap16(0x0002);
    }
    else
    {
        memcpy(&messageInfo.GetPeerAddr(), destination, sizeof(messageInfo.GetPeerAddr()));
    }

    messageInfo.mPeerPort = kCoapUdpPort;
    messageInfo.mInterfaceId = mNetif->GetInterfaceId();

    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    dprintf("Sent address error\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void AddressResolver::HandleAddressError(void *context, Coap::Header &header,
                                         Message &message, const Ip6MessageInfo &messageInfo)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleAddressError(header, message, messageInfo);
}

void AddressResolver::HandleAddressError(Coap::Header &header, Message &message,
                                         const Ip6MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    ThreadTargetTlv targetTlv;
    ThreadMeshLocalIidTlv mlIidTlv;
    Child *children;
    uint8_t numChildren;
    Mac::Address64 macAddr;
    Ip6Address destination;

    VerifyOrExit(header.GetCode() == Coap::Header::kCodePost, error = kThreadError_Drop);

    dprintf("Received address error notification\n");

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kTarget, sizeof(targetTlv), targetTlv));
    VerifyOrExit(targetTlv.IsValid(), error = kThreadError_Parse);

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kMeshLocalIid, sizeof(mlIidTlv), mlIidTlv));
    VerifyOrExit(mlIidTlv.IsValid(), error = kThreadError_Parse);

    for (const NetifUnicastAddress *address = mNetif->GetUnicastAddresses(); address; address = address->GetNext())
    {
        if (memcmp(&address->mAddress, targetTlv.GetTarget(), sizeof(address->mAddress)) == 0 &&
            memcmp(mMle->GetMeshLocal64()->m8 + 8, mlIidTlv.GetIid(), 8))
        {
            // Target EID matches address and Mesh Local EID differs
            mNetif->RemoveUnicastAddress(*address);
            ExitNow();
        }
    }

    children = mMle->GetChildren(&numChildren);

    memcpy(&macAddr, mlIidTlv.GetIid(), sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    for (int i = 0; i < Mle::kMaxChildren; i++)
    {
        if (children[i].mState != Neighbor::kStateValid || (children[i].mMode & Mle::kModeFFD) != 0)
        {
            continue;
        }

        for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++)
        {
            if (memcmp(&children[i].mIp6Address[j], targetTlv.GetTarget(), sizeof(children[i].mIp6Address[j])) == 0 &&
                memcmp(&children[i].mMacAddr, &macAddr, sizeof(children[i].mMacAddr)))
            {
                // Target EID matches child address and Mesh Local EID differs on child
                memset(&children[i].mIp6Address[j], 0, sizeof(children[i].mIp6Address[j]));

                memset(&destination, 0, sizeof(destination));
                destination.m16[0] = HostSwap16(0xfe80);
                memcpy(destination.m8 + 8, &children[i].mMacAddr, 8);
                destination.m8[8] ^= 0x2;

                SendAddressError(targetTlv, mlIidTlv, &destination);
                ExitNow();
            }
        }
    }

exit:
    {}
}

void AddressResolver::HandleAddressQuery(void *context, Coap::Header &header, Message &message,
                                         const Ip6MessageInfo &messageInfo)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleAddressQuery(header, message, messageInfo);
}

void AddressResolver::HandleAddressQuery(Coap::Header &header, Message &message,
                                         const Ip6MessageInfo &messageInfo)
{
    ThreadTargetTlv targetTlv;
    ThreadMeshLocalIidTlv mlIidTlv;
    ThreadLastTransactionTimeTlv lastTransactionTimeTlv;
    Child *children;
    uint8_t numChildren;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeNonConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address query from %04x\n", HostSwap16(messageInfo.GetPeerAddr().m16[7]));

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kTarget, sizeof(targetTlv), targetTlv));
    VerifyOrExit(targetTlv.IsValid(), ;);

    mlIidTlv.Init();

    lastTransactionTimeTlv.Init();

    if (mNetif->IsUnicastAddress(*targetTlv.GetTarget()))
    {
        mlIidTlv.SetIid(mMle->GetMeshLocal64()->m8 + 8);
        SendAddressQueryResponse(targetTlv, mlIidTlv, NULL, messageInfo.GetPeerAddr());
        ExitNow();
    }

    children = mMle->GetChildren(&numChildren);

    for (int i = 0; i < Mle::kMaxChildren; i++)
    {
        if (children[i].mState != Neighbor::kStateValid || (children[i].mMode & Mle::kModeFFD) != 0)
        {
            continue;
        }

        for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++)
        {
            if (memcmp(&children[i].mIp6Address[j], targetTlv.GetTarget(), sizeof(children[i].mIp6Address[j])))
            {
                continue;
            }

            children[i].mMacAddr.mBytes[0] ^= 0x2;
            mlIidTlv.SetIid(children[i].mMacAddr.mBytes);
            children[i].mMacAddr.mBytes[0] ^= 0x2;
            lastTransactionTimeTlv.SetTime(Timer::GetNow() - children[i].mLastHeard);
            SendAddressQueryResponse(targetTlv, mlIidTlv, &lastTransactionTimeTlv, messageInfo.GetPeerAddr());
            ExitNow();
        }
    }

exit:
    {}
}

void AddressResolver::SendAddressQueryResponse(const ThreadTargetTlv &targetTlv,
                                               const ThreadMeshLocalIidTlv &mlIidTlv,
                                               const ThreadLastTransactionTimeTlv *lastTransactionTimeTlv,
                                               const Ip6Address &destination)
{
    ThreadError error;
    Message *message;
    Coap::Header header;
    ThreadRlocTlv rlocTlv;
    Ip6MessageInfo messageInfo;

    VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.Init();
    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions("a/an");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    SuccessOrExit(error = message->Append(&targetTlv, sizeof(targetTlv)));
    SuccessOrExit(error = message->Append(&mlIidTlv, sizeof(mlIidTlv)));

    rlocTlv.Init();
    rlocTlv.SetRloc16(mMle->GetRloc16());
    SuccessOrExit(error = message->Append(&rlocTlv, sizeof(rlocTlv)));

    if (lastTransactionTimeTlv != NULL)
    {
        SuccessOrExit(error = message->Append(&lastTransactionTimeTlv, sizeof(*lastTransactionTimeTlv)));
    }

    memset(&messageInfo, 0, sizeof(messageInfo));
    memcpy(&messageInfo.GetPeerAddr(), &destination, sizeof(messageInfo.GetPeerAddr()));
    messageInfo.mInterfaceId = messageInfo.mInterfaceId;
    messageInfo.mPeerPort = kCoapUdpPort;

    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    dprintf("Sent address notification\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

void AddressResolver::HandleTimer(void *context)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleTimer();
}

void AddressResolver::HandleTimer()
{
    bool continueTimer = false;

    for (int i = 0; i < kCacheEntries; i++)
    {
        switch (mCache[i].mState)
        {
        case Cache::kStateDiscover:
            mCache[i].mTimeout--;

            if (mCache[i].mTimeout == 0)
            {
                mCache[i].mState = Cache::kStateInvalid;
            }
            else
            {
                continueTimer = true;
            }

            break;

        default:
            break;
        }
    }

    if (continueTimer)
    {
        mTimer.Start(1000);
    }
}

const AddressResolver::Cache *AddressResolver::GetCacheEntries(uint16_t *numEntries) const
{
    if (numEntries)
    {
        *numEntries = kCacheEntries;
    }

    return mCache;
}

void AddressResolver::HandleDstUnreach(void *context, Message &message, const Ip6MessageInfo &messageInfo,
                                       const Icmp6Header &icmp6Header)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(context);
    obj->HandleDstUnreach(message, messageInfo, icmp6Header);
}

void AddressResolver::HandleDstUnreach(Message &message, const Ip6MessageInfo &messageInfo,
                                       const Icmp6Header &icmp6Header)
{
    VerifyOrExit(icmp6Header.GetCode() == Icmp6Header::kCodeDstUnreachNoRoute, ;);

    Ip6Header ip6Header;
    VerifyOrExit(message.Read(message.GetOffset(), sizeof(ip6Header), &ip6Header) == sizeof(ip6Header), ;);

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (mCache[i].mState != Cache::kStateInvalid &&
            memcmp(&mCache[i].mTarget, ip6Header.GetDestination(), sizeof(mCache[i].mTarget)) == 0)
        {
            mCache[i].mState = Cache::kStateInvalid;
            dprintf("cache entry removed!\n");
            break;
        }
    }

exit:
    {}
}

}  // namespace Thread
