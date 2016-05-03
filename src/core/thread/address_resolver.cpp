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
#include <common/debug.hpp>
#include <common/logging.hpp>
#include <common/encoding.hpp>
#include <common/thread_error.hpp>
#include <mac/mac_frame.hpp>
#include <platform/random.h>
#include <thread/address_resolver.hpp>
#include <thread/mesh_forwarder.hpp>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>
#include <thread/thread_tlvs.hpp>
#include <thread/thread_uris.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

AddressResolver::AddressResolver(ThreadNetif &netif) :
    mAddressError(OPENTHREAD_URI_ADDRESS_ERROR, &HandleAddressError, this),
    mAddressQuery(OPENTHREAD_URI_ADDRESS_QUERY, &HandleAddressQuery, this),
    mAddressNotification(OPENTHREAD_URI_ADDRESS_NOTIFY, &HandleAddressNotification, this),
    mIcmpHandler(&HandleDstUnreach, this),
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

    Ip6::Icmp::RegisterCallbacks(mIcmpHandler);
}

void AddressResolver::Clear()
{
    memset(&mCache, 0, sizeof(mCache));
}

void AddressResolver::Remove(uint8_t routerId)
{
    for (int i = 0; i < kCacheEntries; i++)
    {
        if ((mCache[i].mRloc16 >> 10) == routerId)
        {
            mCache[i].mState = Cache::kStateInvalid;
        }
    }
}

ThreadError AddressResolver::Resolve(const Ip6::Address &aEid, uint16_t &aRloc16)
{
    ThreadError error = kThreadError_None;
    Cache *entry = NULL;

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (mCache[i].mState != Cache::kStateInvalid)
        {
            if (memcmp(&mCache[i].mTarget, &aEid, sizeof(mCache[i].mTarget)) == 0)
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
        memcpy(&entry->mTarget, &aEid, sizeof(entry->mTarget));
        entry->mState = Cache::kStateDiscover;
        entry->mTimeout = kAddressQueryTimeout;
        mTimer.Start(kStateUpdatePeriod);
        SendAddressQuery(aEid);
        error = kThreadError_AddressQuery;
        break;

    case Cache::kStateDiscover:
    case Cache::kStateRetry:
        error = kThreadError_AddressQuery;
        break;

    case Cache::kStateValid:
        aRloc16 = entry->mRloc16;
        break;
    }

exit:
    return error;
}

ThreadError AddressResolver::SendAddressQuery(const Ip6::Address &aEid)
{
    ThreadError error;
    Ip6::SockAddr sockaddr = {};
    Message *message;
    Coap::Header header;
    ThreadTargetTlv targetTlv;
    Ip6::MessageInfo messageInfo;

    sockaddr.mPort = kCoapUdpPort;
    mSocket.Open(&HandleUdpReceive, this);
    mSocket.Bind(sockaddr);

    for (size_t i = 0; i < sizeof(mCoapToken); i++)
    {
        mCoapToken[i] = otRandomGet();
    }

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.Init();
    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeNonConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions(OPENTHREAD_URI_ADDRESS_QUERY);
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    targetTlv.Init();
    targetTlv.SetTarget(aEid);
    SuccessOrExit(error = message->Append(&targetTlv, sizeof(targetTlv)));

    memset(&messageInfo, 0, sizeof(messageInfo));
    messageInfo.GetPeerAddr().m16[0] = HostSwap16(0xff03);
    messageInfo.GetPeerAddr().m16[7] = HostSwap16(0x0002);
    messageInfo.mPeerPort = kCoapUdpPort;
    messageInfo.mInterfaceId = mNetif->GetInterfaceId();

    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    otLogInfoArp("Sent address query\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void AddressResolver::HandleUdpReceive(void *aContext, otMessage aMessage, const otMessageInfo *aMessageInfo)
{
}

void AddressResolver::HandleAddressNotification(void *aContext, Coap::Header &aHeader, Message &aMessage,
                                                const Ip6::MessageInfo &aMessageInfo)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(aContext);
    obj->HandleAddressNotification(aHeader, aMessage, aMessageInfo);
}

void AddressResolver::HandleAddressNotification(Coap::Header &aHeader, Message &aMessage,
                                                const Ip6::MessageInfo &aMessageInfo)
{
    ThreadTargetTlv targetTlv;
    ThreadMeshLocalEidTlv mlIidTlv;
    ThreadRloc16Tlv rloc16Tlv;

    VerifyOrExit(aHeader.GetType() == Coap::Header::kTypeConfirmable &&
                 aHeader.GetCode() == Coap::Header::kCodePost, ;);

    otLogInfoArp("Received address notification from %04x\n", HostSwap16(aMessageInfo.GetPeerAddr().m16[7]));

    SuccessOrExit(ThreadTlv::GetTlv(aMessage, ThreadTlv::kTarget, sizeof(targetTlv), targetTlv));
    VerifyOrExit(targetTlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(aMessage, ThreadTlv::kMeshLocalEid, sizeof(mlIidTlv), mlIidTlv));
    VerifyOrExit(mlIidTlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(aMessage, ThreadTlv::kRloc16, sizeof(rloc16Tlv), rloc16Tlv));
    VerifyOrExit(rloc16Tlv.IsValid(), ;);

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (memcmp(&mCache[i].mTarget, targetTlv.GetTarget(), sizeof(mCache[i].mTarget)) == 0)
        {
            if (mCache[i].mState != Cache::kStateValid ||
                memcmp(mCache[i].mIid, mlIidTlv.GetIid(), sizeof(mCache[i].mIid)) == 0)
            {
                memcpy(mCache[i].mIid, mlIidTlv.GetIid(), sizeof(mCache[i].mIid));
                mCache[i].mRloc16 = rloc16Tlv.GetRloc16();
                mCache[i].mTimeout = 0;
                mCache[i].mFailureCount = 0;
                mCache[i].mState = Cache::kStateValid;
                SendAddressNotificationResponse(aHeader, aMessageInfo);
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

void AddressResolver::SendAddressNotificationResponse(const Coap::Header &aRequestHeader,
                                                      const Ip6::MessageInfo &aRequestInfo)
{
    ThreadError error;
    Message *message;
    Coap::Header responseHeader;
    Ip6::MessageInfo responseInfo;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    responseHeader.Init();
    responseHeader.SetVersion(1);
    responseHeader.SetType(Coap::Header::kTypeAcknowledgment);
    responseHeader.SetCode(Coap::Header::kCodeChanged);
    responseHeader.SetMessageId(aRequestHeader.GetMessageId());
    responseHeader.SetToken(aRequestHeader.GetToken(), aRequestHeader.GetTokenLength());
    responseHeader.Finalize();
    SuccessOrExit(error = message->Append(responseHeader.GetBytes(), responseHeader.GetLength()));

    memcpy(&responseInfo, &aRequestInfo, sizeof(responseInfo));
    memset(&responseInfo.mSockAddr, 0, sizeof(responseInfo.mSockAddr));
    SuccessOrExit(error = mCoapServer->SendMessage(*message, responseInfo));

    otLogInfoArp("Sent address notification acknowledgment\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError AddressResolver::SendAddressError(const ThreadTargetTlv &aTarget, const ThreadMeshLocalEidTlv &aEid,
                                              const Ip6::Address *aDestination)
{
    ThreadError error;
    Message *message;
    Coap::Header header;
    Ip6::MessageInfo messageInfo;
    Ip6::SockAddr sockaddr = {};

    sockaddr.mPort = kCoapUdpPort;
    mSocket.Open(&HandleUdpReceive, this);
    mSocket.Bind(sockaddr);

    for (size_t i = 0; i < sizeof(mCoapToken); i++)
    {
        mCoapToken[i] = otRandomGet();
    }

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.Init();
    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeNonConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions(OPENTHREAD_URI_ADDRESS_ERROR);
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));
    SuccessOrExit(error = message->Append(&aTarget, sizeof(aTarget)));
    SuccessOrExit(error = message->Append(&aEid, sizeof(aEid)));

    memset(&messageInfo, 0, sizeof(messageInfo));

    if (aDestination == NULL)
    {
        messageInfo.GetPeerAddr().m16[0] = HostSwap16(0xff03);
        messageInfo.GetPeerAddr().m16[7] = HostSwap16(0x0002);
    }
    else
    {
        memcpy(&messageInfo.GetPeerAddr(), aDestination, sizeof(messageInfo.GetPeerAddr()));
    }

    messageInfo.mPeerPort = kCoapUdpPort;
    messageInfo.mInterfaceId = mNetif->GetInterfaceId();

    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    otLogInfoArp("Sent address error\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

void AddressResolver::HandleAddressError(void *aContext, Coap::Header &aHeader,
                                         Message &aMessage, const Ip6::MessageInfo &aMessageInfo)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(aContext);
    obj->HandleAddressError(aHeader, aMessage, aMessageInfo);
}

void AddressResolver::HandleAddressError(Coap::Header &aHeader, Message &aMessage,
                                         const Ip6::MessageInfo &aMessageInfo)
{
    ThreadError error = kThreadError_None;
    ThreadTargetTlv targetTlv;
    ThreadMeshLocalEidTlv mlIidTlv;
    Child *children;
    uint8_t numChildren;
    Mac::ExtAddress macAddr;
    Ip6::Address destination;

    VerifyOrExit(aHeader.GetCode() == Coap::Header::kCodePost, error = kThreadError_Drop);

    otLogInfoArp("Received address error notification\n");

    SuccessOrExit(error = ThreadTlv::GetTlv(aMessage, ThreadTlv::kTarget, sizeof(targetTlv), targetTlv));
    VerifyOrExit(targetTlv.IsValid(), error = kThreadError_Parse);

    SuccessOrExit(error = ThreadTlv::GetTlv(aMessage, ThreadTlv::kMeshLocalEid, sizeof(mlIidTlv), mlIidTlv));
    VerifyOrExit(mlIidTlv.IsValid(), error = kThreadError_Parse);

    for (const Ip6::NetifUnicastAddress *address = mNetif->GetUnicastAddresses(); address; address = address->GetNext())
    {
        if (memcmp(&address->mAddress, targetTlv.GetTarget(), sizeof(address->mAddress)) == 0 &&
            memcmp(mMle->GetMeshLocal64()->GetIid(), mlIidTlv.GetIid(), 8))
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
        if (children[i].mState != Neighbor::kStateValid || (children[i].mMode & Mle::ModeTlv::kModeFFD) != 0)
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
                destination.SetIid(children[i].mMacAddr);

                SendAddressError(targetTlv, mlIidTlv, &destination);
                ExitNow();
            }
        }
    }

exit:
    {}
}

void AddressResolver::HandleAddressQuery(void *aContext, Coap::Header &aHeader, Message &aMessage,
                                         const Ip6::MessageInfo &aMessageInfo)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(aContext);
    obj->HandleAddressQuery(aHeader, aMessage, aMessageInfo);
}

void AddressResolver::HandleAddressQuery(Coap::Header &aHeader, Message &aMessage,
                                         const Ip6::MessageInfo &aMessageInfo)
{
    ThreadTargetTlv targetTlv;
    ThreadMeshLocalEidTlv mlIidTlv;
    ThreadLastTransactionTimeTlv lastTransactionTimeTlv;
    Child *children;
    uint8_t numChildren;

    VerifyOrExit(aHeader.GetType() == Coap::Header::kTypeNonConfirmable &&
                 aHeader.GetCode() == Coap::Header::kCodePost, ;);

    otLogInfoArp("Received address query from %04x\n", HostSwap16(aMessageInfo.GetPeerAddr().m16[7]));

    SuccessOrExit(ThreadTlv::GetTlv(aMessage, ThreadTlv::kTarget, sizeof(targetTlv), targetTlv));
    VerifyOrExit(targetTlv.IsValid(), ;);

    mlIidTlv.Init();

    lastTransactionTimeTlv.Init();

    if (mNetif->IsUnicastAddress(*targetTlv.GetTarget()))
    {
        mlIidTlv.SetIid(mMle->GetMeshLocal64()->GetIid());
        SendAddressQueryResponse(targetTlv, mlIidTlv, NULL, aMessageInfo.GetPeerAddr());
        ExitNow();
    }

    children = mMle->GetChildren(&numChildren);

    for (int i = 0; i < Mle::kMaxChildren; i++)
    {
        if (children[i].mState != Neighbor::kStateValid || (children[i].mMode & Mle::ModeTlv::kModeFFD) != 0)
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
            SendAddressQueryResponse(targetTlv, mlIidTlv, &lastTransactionTimeTlv, aMessageInfo.GetPeerAddr());
            ExitNow();
        }
    }

exit:
    {}
}

void AddressResolver::SendAddressQueryResponse(const ThreadTargetTlv &aTargetTlv,
                                               const ThreadMeshLocalEidTlv &aMlIidTlv,
                                               const ThreadLastTransactionTimeTlv *aLastTransactionTimeTlv,
                                               const Ip6::Address &aDestination)
{
    ThreadError error;
    Message *message;
    Coap::Header header;
    ThreadRloc16Tlv rloc16Tlv;
    Ip6::MessageInfo messageInfo;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);

    header.Init();
    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(NULL, 0);
    header.AppendUriPathOptions(OPENTHREAD_URI_ADDRESS_NOTIFY);
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    SuccessOrExit(error = message->Append(&aTargetTlv, sizeof(aTargetTlv)));
    SuccessOrExit(error = message->Append(&aMlIidTlv, sizeof(aMlIidTlv)));

    rloc16Tlv.Init();
    rloc16Tlv.SetRloc16(mMle->GetRloc16());
    SuccessOrExit(error = message->Append(&rloc16Tlv, sizeof(rloc16Tlv)));

    if (aLastTransactionTimeTlv != NULL)
    {
        SuccessOrExit(error = message->Append(aLastTransactionTimeTlv, sizeof(*aLastTransactionTimeTlv)));
    }

    memset(&messageInfo, 0, sizeof(messageInfo));
    memcpy(&messageInfo.GetPeerAddr(), &aDestination, sizeof(messageInfo.GetPeerAddr()));
    messageInfo.mInterfaceId = messageInfo.mInterfaceId;
    messageInfo.mPeerPort = kCoapUdpPort;

    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    otLogInfoArp("Sent address notification\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

void AddressResolver::HandleTimer(void *aContext)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(aContext);
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
        mTimer.Start(kStateUpdatePeriod);
    }
}

void AddressResolver::HandleDstUnreach(void *aContext, Message &aMessage, const Ip6::MessageInfo &aMessageInfo,
                                       const Ip6::IcmpHeader &aIcmpHeader)
{
    AddressResolver *obj = reinterpret_cast<AddressResolver *>(aContext);
    obj->HandleDstUnreach(aMessage, aMessageInfo, aIcmpHeader);
}

void AddressResolver::HandleDstUnreach(Message &aMessage, const Ip6::MessageInfo &aMessageInfo,
                                       const Ip6::IcmpHeader &aIcmpHeader)
{
    VerifyOrExit(aIcmpHeader.GetCode() == Ip6::IcmpHeader::kCodeDstUnreachNoRoute, ;);

    Ip6::Header ip6Header;
    VerifyOrExit(aMessage.Read(aMessage.GetOffset(), sizeof(ip6Header), &ip6Header) == sizeof(ip6Header), ;);

    for (int i = 0; i < kCacheEntries; i++)
    {
        if (mCache[i].mState != Cache::kStateInvalid &&
            memcmp(&mCache[i].mTarget, &ip6Header.GetDestination(), sizeof(mCache[i].mTarget)) == 0)
        {
            mCache[i].mState = Cache::kStateInvalid;
            otLogInfoArp("cache entry removed!\n");
            break;
        }
    }

exit:
    {}
}

}  // namespace Thread
