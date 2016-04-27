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
 *   This file implements MLE functionality required for the Thread Child, Router and Leader roles.
 */

#include <assert.h>

#include <thread/mle.hpp>
#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <crypto/aes_ccm.hpp>
#include <mac/mac_frame.hpp>
#include <net/netif.hpp>
#include <net/udp6.hpp>
#include <platform/random.h>
#include <thread/address_resolver.hpp>
#include <thread/key_manager.hpp>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Mle {

Mle::Mle(ThreadNetif &netif) :
    mNetifHandler(&HandleUnicastAddressesChanged, this),
    mParentRequestTimer(&HandleParentRequestTimer, this)
{
    mNetif = &netif;
    mAddressResolver = netif.GetAddressResolver();
    mKeyManager = netif.GetKeyManager();
    mMesh = netif.GetMeshForwarder();
    mMleRouter = netif.GetMle();
    mNetworkData = netif.GetNetworkDataLeader();
}

ThreadError Mle::Init()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mDeviceState == kDeviceStateDisabled, error = kThreadError_Busy);

    memset(&mLeaderData, 0, sizeof(mLeaderData));
    memset(&mParent, 0, sizeof(mParent));
    memset(&mChildIdRequest, 0, sizeof(mChildIdRequest));
    memset(&mLinkLocal64, 0, sizeof(mLinkLocal64));
    memset(&mLinkLocal16, 0, sizeof(mLinkLocal16));
    memset(&mMeshLocal64, 0, sizeof(mMeshLocal64));
    memset(&mMeshLocal16, 0, sizeof(mMeshLocal16));
    memset(&mLinkLocalAllThreadNodes, 0, sizeof(mLinkLocalAllThreadNodes));
    memset(&mRealmLocalAllThreadNodes, 0, sizeof(mRealmLocalAllThreadNodes));

    // link-local 64
    memset(&mLinkLocal64, 0, sizeof(mLinkLocal64));
    mLinkLocal64.GetAddress().m16[0] = HostSwap16(0xfe80);
    memcpy(mLinkLocal64.GetAddress().m8 + 8, mMesh->GetExtAddress(), 8);
    mLinkLocal64.GetAddress().m8[8] ^= 2;
    mLinkLocal64.mPrefixLength = 64;
    mLinkLocal64.mPreferredLifetime = 0xffffffff;
    mLinkLocal64.mValidLifetime = 0xffffffff;
    mNetif->AddUnicastAddress(mLinkLocal64);

    // link-local 16
    memset(&mLinkLocal16, 0, sizeof(mLinkLocal16));
    mLinkLocal16.GetAddress().m16[0] = HostSwap16(0xfe80);
    mLinkLocal16.GetAddress().m16[5] = HostSwap16(0x00ff);
    mLinkLocal16.GetAddress().m16[6] = HostSwap16(0xfe00);
    mLinkLocal16.mPrefixLength = 64;
    mLinkLocal16.mPreferredLifetime = 0xffffffff;
    mLinkLocal16.mValidLifetime = 0xffffffff;

    // mesh-local 64
    for (int i = 8; i < 16; i++)
    {
        mMeshLocal64.GetAddress().m8[i] = otRandomGet();
    }

    mMeshLocal64.mPrefixLength = 64;
    mMeshLocal64.mPreferredLifetime = 0xffffffff;
    mMeshLocal64.mValidLifetime = 0xffffffff;
    mNetif->AddUnicastAddress(mMeshLocal64);

    // mesh-local 16
    mMeshLocal16.GetAddress().m16[4] = HostSwap16(0x0000);
    mMeshLocal16.GetAddress().m16[5] = HostSwap16(0x00ff);
    mMeshLocal16.GetAddress().m16[6] = HostSwap16(0xfe00);
    mMeshLocal16.mPrefixLength = 64;
    mMeshLocal16.mPreferredLifetime = 0xffffffff;
    mMeshLocal16.mValidLifetime = 0xffffffff;

    // link-local all thread nodes
    mLinkLocalAllThreadNodes.GetAddress().m16[0] = HostSwap16(0xff32);
    mLinkLocalAllThreadNodes.GetAddress().m16[6] = HostSwap16(0x0000);
    mLinkLocalAllThreadNodes.GetAddress().m16[7] = HostSwap16(0x0001);
    mNetif->SubscribeMulticast(mLinkLocalAllThreadNodes);

    // realm-local all thread nodes
    mRealmLocalAllThreadNodes.GetAddress().m16[0] = HostSwap16(0xff33);
    mRealmLocalAllThreadNodes.GetAddress().m16[6] = HostSwap16(0x0000);
    mRealmLocalAllThreadNodes.GetAddress().m16[7] = HostSwap16(0x0001);
    mNetif->SubscribeMulticast(mRealmLocalAllThreadNodes);

    mNetif->RegisterHandler(mNetifHandler);

exit:
    return error;
}

ThreadError Mle::Start()
{
    ThreadError error = kThreadError_None;
    Ip6::SockAddr sockaddr = {};

    // memcpy(&sockaddr.mAddr, &mLinkLocal64.GetAddress(), sizeof(sockaddr.mAddr));
    sockaddr.mPort = kUdpPort;
    SuccessOrExit(error = mSocket.Open(&HandleUdpReceive, this));
    SuccessOrExit(error = mSocket.Bind(sockaddr));

    mDeviceState = kDeviceStateDetached;
    SetStateDetached();

    if (GetRloc16() == Mac::kShortAddrInvalid)
    {
        BecomeChild(kMleAttachAnyPartition);
    }
    else if (GetChildId(GetRloc16()) == 0)
    {
        mMleRouter->BecomeRouter();
    }
    else
    {
        SendChildUpdateRequest();
        mParentRequestState = kParentSynchronize;
        mParentRequestTimer.Start(1000);
    }

exit:
    return error;
}

ThreadError Mle::Stop()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mDeviceState != kDeviceStateDisabled, error = kThreadError_Busy);

    SetStateDetached();
    mSocket.Close();
    mNetif->RemoveUnicastAddress(mLinkLocal16);
    mNetif->RemoveUnicastAddress(mMeshLocal16);
    mDeviceState = kDeviceStateDisabled;

exit:
    return error;
}

ThreadError Mle::BecomeDetached()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mDeviceState != kDeviceStateDisabled, error = kThreadError_Busy);

    SetStateDetached();
    SetRloc16(Mac::kShortAddrInvalid);
    BecomeChild(kMleAttachAnyPartition);

exit:
    return error;
}

ThreadError Mle::BecomeChild(otMleAttachFilter filter)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mDeviceState != kDeviceStateDisabled &&
                 mParentRequestState == kParentIdle, error = kThreadError_Busy);

    mParentRequestState = kParentRequestStart;
    mParentRequestMode = filter;
    memset(&mParent, 0, sizeof(mParent));

    if (filter == kMleAttachAnyPartition)
    {
        mParent.mState = Neighbor::kStateInvalid;
    }

    mParentRequestTimer.Start(1000);

exit:
    return error;
}

DeviceState Mle::GetDeviceState() const
{
    return mDeviceState;
}

ThreadError Mle::SetStateDetached()
{
    mAddressResolver->Clear();
    mDeviceState = kDeviceStateDetached;
    mParentRequestState = kParentIdle;
    mParentRequestTimer.Stop();
    mMesh->SetRxOnWhenIdle(true);
    mMleRouter->HandleDetachStart();
    dprintf("Mode -> Detached\n");
    return kThreadError_None;
}

ThreadError Mle::SetStateChild(uint16_t rloc16)
{
    SetRloc16(rloc16);
    mDeviceState = kDeviceStateChild;
    mParentRequestState = kParentIdle;

    if ((mDeviceMode & kModeRxOnWhenIdle) != 0)
    {
        mParentRequestTimer.Start((mTimeout / 2) * 1000U);
    }

    if ((mDeviceMode & kModeFFD) != 0)
    {
        mMleRouter->HandleChildStart(mParentRequestMode);
    }

    dprintf("Mode -> Child\n");
    return kThreadError_None;
}

uint32_t Mle::GetTimeout() const
{
    return mTimeout;
}

ThreadError Mle::SetTimeout(uint32_t timeout)
{
    if (timeout < 2)
    {
        timeout = 2;
    }

    mTimeout = timeout;

    if (mDeviceState == kDeviceStateChild)
    {
        SendChildUpdateRequest();

        if ((mDeviceMode & kModeRxOnWhenIdle) != 0)
        {
            mParentRequestTimer.Start((mTimeout / 2) * 1000U);
        }
    }

    return kThreadError_None;
}

uint8_t Mle::GetDeviceMode() const
{
    return mDeviceMode;
}

ThreadError Mle::SetDeviceMode(uint8_t deviceMode)
{
    ThreadError error = kThreadError_None;
    uint8_t oldMode = mDeviceMode;

    VerifyOrExit((deviceMode & kModeFFD) == 0 || (deviceMode & kModeRxOnWhenIdle) != 0,
                 error = kThreadError_InvalidArgs);

    mDeviceMode = deviceMode;

    switch (mDeviceState)
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        break;

    case kDeviceStateChild:
        SetStateChild(GetRloc16());
        SendChildUpdateRequest();
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        if ((oldMode & kModeFFD) != 0 && (deviceMode & kModeFFD) == 0)
        {
            BecomeDetached();
        }

        break;
    }

exit:
    return error;
}

const uint8_t *Mle::GetMeshLocalPrefix() const
{
    return mMeshLocal16.GetAddress().m8;
}

ThreadError Mle::SetMeshLocalPrefix(const uint8_t *xpanid)
{
    mMeshLocal64.GetAddress().m8[0] = 0xfd;
    memcpy(mMeshLocal64.GetAddress().m8 + 1, xpanid, 5);
    mMeshLocal64.GetAddress().m8[6] = 0x00;
    mMeshLocal64.GetAddress().m8[7] = 0x00;

    memcpy(&mMeshLocal16.GetAddress(), &mMeshLocal64.GetAddress(), 8);

    mLinkLocalAllThreadNodes.GetAddress().m8[3] = 64;
    memcpy(mLinkLocalAllThreadNodes.GetAddress().m8 + 4, &mMeshLocal64.GetAddress(), 8);

    mRealmLocalAllThreadNodes.GetAddress().m8[3] = 64;
    memcpy(mRealmLocalAllThreadNodes.GetAddress().m8 + 4, &mMeshLocal64.GetAddress(), 8);

    return kThreadError_None;
}

const uint8_t Mle::GetChildId(uint16_t rloc16) const
{
    return rloc16 & kChildIdMask;
}

const uint8_t Mle::GetRouterId(uint16_t rloc16) const
{
    return rloc16 >> kRouterIdOffset;
}

const uint16_t Mle::GetRloc16(uint8_t routerId) const
{
    return static_cast<uint16_t>(routerId) << kRouterIdOffset;
}

const Ip6::Address *Mle::GetLinkLocalAllThreadNodesAddress() const
{
    return &mLinkLocalAllThreadNodes.GetAddress();
}

const Ip6::Address *Mle::GetRealmLocalAllThreadNodesAddress() const
{
    return &mRealmLocalAllThreadNodes.GetAddress();
}

uint16_t Mle::GetRloc16() const
{
    return mMesh->GetShortAddress();
}

ThreadError Mle::SetRloc16(uint16_t rloc16)
{
    if (rloc16 != Mac::kShortAddrInvalid)
    {
        // link-local 16
        mLinkLocal16.GetAddress().m16[7] = HostSwap16(rloc16);
        mNetif->AddUnicastAddress(mLinkLocal16);

        // mesh-local 16
        mMeshLocal16.GetAddress().m16[7] = HostSwap16(rloc16);
        mNetif->AddUnicastAddress(mMeshLocal16);
    }
    else
    {
        mNetif->RemoveUnicastAddress(mLinkLocal16);
        mNetif->RemoveUnicastAddress(mMeshLocal16);
    }

    mMesh->SetShortAddress(rloc16);

    return kThreadError_None;
}

uint8_t Mle::GetLeaderId() const
{
    return mLeaderData.GetRouterId();
}

const Ip6::Address *Mle::GetMeshLocal16() const
{
    return &mMeshLocal16.GetAddress();
}

const Ip6::Address *Mle::GetMeshLocal64() const
{
    return &mMeshLocal64.GetAddress();
}

ThreadError Mle::GetLeaderAddress(Ip6::Address &address) const
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(GetRloc16() != Mac::kShortAddrInvalid, error = kThreadError_Error);

    memcpy(&address, &mMeshLocal16.GetAddress(), 8);
    address.m16[4] = HostSwap16(0x0000);
    address.m16[5] = HostSwap16(0x00ff);
    address.m16[6] = HostSwap16(0xfe00);
    address.m16[7] = HostSwap16(GetRloc16(mLeaderData.GetRouterId()));

exit:
    return error;
}

const LeaderDataTlv *Mle::GetLeaderDataTlv()
{
    mLeaderData.SetDataVersion(mNetworkData->GetVersion());
    mLeaderData.SetStableDataVersion(mNetworkData->GetStableVersion());
    return &mLeaderData;
}

void Mle::GenerateNonce(const Mac::ExtAddress &macAddr, uint32_t frameCounter, uint8_t securityLevel, uint8_t *nonce)
{
    // source address
    for (int i = 0; i < 8; i++)
    {
        nonce[i] = macAddr.mBytes[i];
    }

    nonce += 8;

    // frame counter
    nonce[0] = frameCounter >> 24;
    nonce[1] = frameCounter >> 16;
    nonce[2] = frameCounter >> 8;
    nonce[3] = frameCounter >> 0;
    nonce += 4;

    // security level
    nonce[0] = securityLevel;
}

ThreadError Mle::AppendSecureHeader(Message &message, Header::Command command)
{
    ThreadError error = kThreadError_None;
    Header header;

    header.Init();

    if (command == Header::kCommandAdvertisement ||
        command == Header::kCommandChildIdRequest ||
        command == Header::kCommandLinkReject ||
        command == Header::kCommandParentRequest ||
        command == Header::kCommandParentResponse)
    {
        header.SetKeyIdMode2();
    }
    else
    {
        header.SetKeyIdMode1();
    }

    header.SetCommand(command);

    SuccessOrExit(error = message.Append(&header, header.GetLength()));

exit:
    return error;
}

ThreadError Mle::AppendSourceAddress(Message &message)
{
    SourceAddressTlv tlv;

    tlv.Init();
    tlv.SetRloc16(GetRloc16());

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendStatus(Message &message, StatusTlv::Status status)
{
    StatusTlv tlv;

    tlv.Init();
    tlv.SetStatus(status);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendMode(Message &message, uint8_t mode)
{
    ModeTlv tlv;

    tlv.Init();
    tlv.SetMode(mode);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendTimeout(Message &message, uint32_t timeout)
{
    TimeoutTlv tlv;

    tlv.Init();
    tlv.SetTimeout(timeout);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendChallenge(Message &message, const uint8_t *challenge, uint8_t challengeLength)
{
    ThreadError error;
    Tlv tlv;

    tlv.SetType(Tlv::kChallenge);
    tlv.SetLength(challengeLength);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));
    SuccessOrExit(error = message.Append(challenge, challengeLength));
exit:
    return error;
}

ThreadError Mle::AppendResponse(Message &message, const uint8_t *response, uint8_t responseLength)
{
    ThreadError error;
    Tlv tlv;

    tlv.SetType(Tlv::kResponse);
    tlv.SetLength(responseLength);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));
    SuccessOrExit(error = message.Append(response, responseLength));

exit:
    return error;
}

ThreadError Mle::AppendLinkFrameCounter(Message &message)
{
    LinkFrameCounterTlv tlv;

    tlv.Init();
    tlv.SetFrameCounter(mKeyManager->GetMacFrameCounter());

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendMleFrameCounter(Message &message)
{
    MleFrameCounterTlv tlv;

    tlv.Init();
    tlv.SetFrameCounter(mKeyManager->GetMleFrameCounter());

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendAddress16(Message &message, uint16_t rloc16)
{
    Address16Tlv tlv;

    tlv.Init();
    tlv.SetRloc16(rloc16);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendLeaderData(Message &message)
{
    mLeaderData.Init();
    mLeaderData.SetDataVersion(mNetworkData->GetVersion());
    mLeaderData.SetStableDataVersion(mNetworkData->GetStableVersion());

    return message.Append(&mLeaderData, sizeof(mLeaderData));
}

ThreadError Mle::AppendNetworkData(Message &message, bool stableOnly)
{
    ThreadError error = kThreadError_None;
    NetworkDataTlv tlv;
    uint8_t length;

    tlv.Init();
    SuccessOrExit(error = mNetworkData->GetNetworkData(stableOnly, tlv.GetNetworkData(), length));
    tlv.SetLength(length);

    SuccessOrExit(error = message.Append(&tlv, sizeof(Tlv) + tlv.GetLength()));

exit:
    return error;
}

ThreadError Mle::AppendTlvRequest(Message &message, const uint8_t *tlvs, uint8_t tlvsLength)
{
    ThreadError error;
    Tlv tlv;

    tlv.SetType(Tlv::kTlvRequest);
    tlv.SetLength(tlvsLength);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));
    SuccessOrExit(error = message.Append(tlvs, tlvsLength));

exit:
    return error;
}

ThreadError Mle::AppendScanMask(Message &message, uint8_t scan_mask)
{
    ScanMaskTlv tlv;

    tlv.Init();
    tlv.SetMask(scan_mask);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendLinkMargin(Message &message, uint8_t linkMargin)
{
    LinkMarginTlv tlv;

    tlv.Init();
    tlv.SetLinkMargin(linkMargin);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendVersion(Message &message)
{
    VersionTlv tlv;

    tlv.Init();
    tlv.SetVersion(kVersion);

    return message.Append(&tlv, sizeof(tlv));
}

ThreadError Mle::AppendIp6Address(Message &message)
{
    ThreadError error;
    Tlv tlv;
    AddressRegistrationEntry entry;
    Context context;
    uint8_t length = 0;

    tlv.SetType(Tlv::kAddressRegistration);

    // compute size of TLV
    for (const Ip6::NetifUnicastAddress *addr = mNetif->GetUnicastAddresses(); addr; addr = addr->GetNext())
    {
        if (addr->GetAddress().IsLinkLocal() || addr->GetAddress() == mMeshLocal16.GetAddress())
        {
            continue;
        }

        if (mNetworkData->GetContext(addr->GetAddress(), context) == kThreadError_None)
        {
            // compressed entry
            length += 9;
        }
        else
        {
            // uncompressed entry
            length += 17;
        }
    }

    tlv.SetLength(length);
    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));

    // write entries to message
    for (const Ip6::NetifUnicastAddress *addr = mNetif->GetUnicastAddresses(); addr; addr = addr->GetNext())
    {
        if (addr->GetAddress().IsLinkLocal() || addr->GetAddress() == mMeshLocal16.GetAddress())
        {
            continue;
        }

        if (mNetworkData->GetContext(addr->GetAddress(), context) == kThreadError_None)
        {
            // compressed entry
            entry.SetContextId(context.mContextId);
            entry.SetIid(addr->GetAddress().m8 + 8);
            length = 9;
        }
        else
        {
            // uncompressed entry
            entry.SetUncompressed();
            entry.SetIp6Address(addr->GetAddress());
            length = 17;
        }

        SuccessOrExit(error = message.Append(&entry, length));
    }

exit:
    return error;
}

void Mle::HandleUnicastAddressesChanged(void *context)
{
    Mle *obj = reinterpret_cast<Mle *>(context);
    obj->HandleUnicastAddressesChanged();
}

void Mle::HandleUnicastAddressesChanged()
{
    if (!mNetif->IsUnicastAddress(mMeshLocal64.GetAddress()))
    {
        // Mesh Local EID was removed, choose a new one and add it back
        for (int i = 8; i < 16; i++)
        {
            mMeshLocal64.GetAddress().m8[i] = otRandomGet();
        }

        mNetif->AddUnicastAddress(mMeshLocal64);
    }

    switch (mDeviceState)
    {
    case kDeviceStateChild:
        SendChildUpdateRequest();
        break;

    default:
        break;
    }
}

void Mle::HandleParentRequestTimer(void *context)
{
    Mle *obj = reinterpret_cast<Mle *>(context);
    obj->HandleParentRequestTimer();
}

void Mle::HandleParentRequestTimer()
{
    switch (mParentRequestState)
    {
    case kParentIdle:
        if (mParent.mState == Neighbor::kStateValid)
        {
            if (mDeviceMode & kModeRxOnWhenIdle)
            {
                SendChildUpdateRequest();
                mParentRequestTimer.Start((mTimeout / 2) * 1000U);
            }
        }
        else
        {
            BecomeDetached();
        }

        break;

    case kParentSynchronize:
        mParentRequestState = kParentIdle;
        BecomeChild(kMleAttachAnyPartition);
        break;

    case kParentRequestStart:
        mParentRequestState = kParentRequestRouter;
        mParent.mState = Neighbor::kStateInvalid;
        SendParentRequest();
        mParentRequestTimer.Start(kParentRequestRouterTimeout);
        break;

    case kParentRequestRouter:
        mParentRequestState = kParentRequestChild;

        if (mParent.mState == Neighbor::kStateValid)
        {
            SendChildIdRequest();
            mParentRequestState = kChildIdRequest;
        }
        else
        {
            SendParentRequest();
        }

        mParentRequestTimer.Start(kParentRequestChildTimeout);
        break;

    case kParentRequestChild:
        mParentRequestState = kParentRequestChild;

        if (mParent.mState == Neighbor::kStateValid)
        {
            SendChildIdRequest();
            mParentRequestState = kChildIdRequest;
            mParentRequestTimer.Start(kParentRequestChildTimeout);
        }
        else
        {
            switch (mParentRequestMode)
            {
            case kMleAttachAnyPartition:
                if (mDeviceMode & kModeFFD)
                {
                    mMleRouter->BecomeLeader();
                }
                else
                {
                    mParentRequestState = kParentIdle;
                    BecomeDetached();
                }

                break;

            case kMleAttachSamePartition:
                mParentRequestState = kParentIdle;
                BecomeChild(kMleAttachAnyPartition);
                break;

            case kMleAttachBetterPartition:
                mParentRequestState = kParentIdle;
                break;
            }
        }

        break;

    case kChildIdRequest:
        mParentRequestState = kParentIdle;

        if (mDeviceState != kDeviceStateRouter && mDeviceState != kDeviceStateLeader)
        {
            BecomeDetached();
        }

        break;
    }
}

ThreadError Mle::SendParentRequest()
{
    ThreadError error = kThreadError_None;
    Message *message;
    uint8_t scanMask = 0;
    Ip6::Address destination;

    for (uint8_t i = 0; i < sizeof(mParentRequest.mChallenge); i++)
    {
        mParentRequest.mChallenge[i] = otRandomGet();
    }

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandParentRequest));
    SuccessOrExit(error = AppendMode(*message, mDeviceMode));
    SuccessOrExit(error = AppendChallenge(*message, mParentRequest.mChallenge, sizeof(mParentRequest.mChallenge)));

    switch (mParentRequestState)
    {
    case kParentRequestRouter:
        scanMask = ScanMaskTlv::kRouterFlag;
        break;

    case kParentRequestChild:
        scanMask = ScanMaskTlv::kRouterFlag | ScanMaskTlv::kChildFlag;
        break;

    default:
        assert(false);
        break;
    }

    SuccessOrExit(error = AppendScanMask(*message, scanMask));
    SuccessOrExit(error = AppendVersion(*message));

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xff02);
    destination.m16[7] = HostSwap16(0x0002);
    SuccessOrExit(error = SendMessage(*message, destination));

    switch (mParentRequestState)
    {
    case kParentRequestRouter:
        dprintf("Sent parent request to routers\n");
        break;

    case kParentRequestChild:
        dprintf("Sent parent request to all devices\n");
        break;

    default:
        assert(false);
        break;
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

ThreadError Mle::SendChildIdRequest()
{
    ThreadError error = kThreadError_None;
    uint8_t tlvs[] = {Tlv::kAddress16, Tlv::kNetworkData, Tlv::kRoute};
    Message *message;
    Ip6::Address destination;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildIdRequest));
    SuccessOrExit(error = AppendResponse(*message, mChildIdRequest.mChallenge, mChildIdRequest.mChallengeLength));
    SuccessOrExit(error = AppendLinkFrameCounter(*message));
    SuccessOrExit(error = AppendMleFrameCounter(*message));
    SuccessOrExit(error = AppendMode(*message, mDeviceMode));
    SuccessOrExit(error = AppendTimeout(*message, mTimeout));
    SuccessOrExit(error = AppendVersion(*message));

    if ((mDeviceMode & kModeFFD) == 0)
    {
        SuccessOrExit(error = AppendIp6Address(*message));
    }

    SuccessOrExit(error = AppendTlvRequest(*message, tlvs, sizeof(tlvs)));

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xfe80);
    memcpy(destination.m8 + 8, &mParent.mMacAddr, sizeof(mParent.mMacAddr));
    destination.m8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));
    dprintf("Sent Child ID Request\n");

    if ((mDeviceMode & kModeRxOnWhenIdle) == 0)
    {
        mMesh->SetPollPeriod(100);
        mMesh->SetRxOnWhenIdle(false);
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendDataRequest(const Ip6::Address &destination, const uint8_t *tlvs, uint8_t tlvsLength)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandDataRequest));
    SuccessOrExit(error = AppendTlvRequest(*message, tlvs, tlvsLength));

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Data Request\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendDataResponse(const Ip6::Address &destination, const uint8_t *tlvs, uint8_t tlvsLength)
{
    ThreadError error = kThreadError_None;
    Message *message;
    Neighbor *neighbor;
    bool stableOnly;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandDataResponse));

    neighbor = mMleRouter->GetNeighbor(destination);

    for (int i = 0; i < tlvsLength; i++)
    {
        switch (tlvs[i])
        {
        case Tlv::kLeaderData:
            SuccessOrExit(error = AppendLeaderData(*message));
            break;

        case Tlv::kNetworkData:
            stableOnly = neighbor != NULL ? (neighbor->mMode & kModeFullNetworkData) == 0 : false;
            SuccessOrExit(error = AppendNetworkData(*message, stableOnly));
            break;
        }
    }

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Data Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendChildUpdateRequest()
{
    ThreadError error = kThreadError_None;
    Ip6::Address destination;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildUpdateRequest));
    SuccessOrExit(error = AppendMode(*message, mDeviceMode));

    if ((mDeviceMode & kModeFFD) == 0)
    {
        SuccessOrExit(error = AppendIp6Address(*message));
    }

    switch (mDeviceState)
    {
    case kDeviceStateDetached:
        for (uint8_t i = 0; i < sizeof(mParentRequest.mChallenge); i++)
        {
            mParentRequest.mChallenge[i] = otRandomGet();
        }

        SuccessOrExit(error = AppendChallenge(*message, mParentRequest.mChallenge,
                                              sizeof(mParentRequest.mChallenge)));
        break;

    case kDeviceStateChild:
        SuccessOrExit(error = AppendSourceAddress(*message));
        SuccessOrExit(error = AppendLeaderData(*message));
        SuccessOrExit(error = AppendTimeout(*message, mTimeout));
        break;

    case kDeviceStateDisabled:
    case kDeviceStateRouter:
    case kDeviceStateLeader:
        assert(false);
        break;
    }

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xfe80);
    memcpy(destination.m8 + 8, &mParent.mMacAddr, sizeof(mParent.mMacAddr));
    destination.m8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Child Update Request\n");

    if ((mDeviceMode & kModeRxOnWhenIdle) == 0)
    {
        mMesh->SetPollPeriod(100);
    }

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError Mle::SendMessage(Message &message, const Ip6::Address &destination)
{
    ThreadError error = kThreadError_None;
    Header header;
    uint32_t keySequence;
    uint8_t nonce[13];
    uint8_t tag[4];
    uint8_t tagLength;
    Crypto::AesEcb aesEcb;
    Crypto::AesCcm aesCcm;
    uint8_t buf[64];
    int length;
    Ip6::MessageInfo messageInfo;

    message.Read(0, sizeof(header), &header);
    header.SetFrameCounter(mKeyManager->GetMleFrameCounter());

    keySequence = mKeyManager->GetCurrentKeySequence();
    header.SetKeyId(keySequence);

    message.Write(0, header.GetLength(), &header);

    GenerateNonce(*mMesh->GetExtAddress(), mKeyManager->GetMleFrameCounter(), Mac::Frame::kSecEncMic32, nonce);

    aesEcb.SetKey(mKeyManager->GetCurrentMleKey(), 16);
    aesCcm.Init(aesEcb, 16 + 16 + header.GetHeaderLength(), message.GetLength() - (header.GetLength() - 1),
                sizeof(tag), nonce, sizeof(nonce));

    aesCcm.Header(&mLinkLocal64.GetAddress(), sizeof(mLinkLocal64.GetAddress()));
    aesCcm.Header(&destination, sizeof(destination));
    aesCcm.Header(header.GetBytes() + 1, header.GetHeaderLength());

    message.SetOffset(header.GetLength() - 1);

    while (message.GetOffset() < message.GetLength())
    {
        length = message.Read(message.GetOffset(), sizeof(buf), buf);
        aesCcm.Payload(buf, buf, length, true);
        message.Write(message.GetOffset(), length, buf);
        message.MoveOffset(length);
    }

    tagLength = sizeof(tag);
    aesCcm.Finalize(tag, &tagLength);
    SuccessOrExit(message.Append(tag, tagLength));

    memset(&messageInfo, 0, sizeof(messageInfo));
    memcpy(&messageInfo.GetPeerAddr(), &destination, sizeof(messageInfo.GetPeerAddr()));
    memcpy(&messageInfo.GetSockAddr(), &mLinkLocal64.GetAddress(), sizeof(messageInfo.GetSockAddr()));
    messageInfo.mPeerPort = kUdpPort;
    messageInfo.mInterfaceId = mNetif->GetInterfaceId();
    messageInfo.mHopLimit = 255;

    mKeyManager->IncrementMleFrameCounter();

    SuccessOrExit(error = mSocket.SendTo(message, messageInfo));

exit:
    return error;
}

void Mle::HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo)
{
    Mle *obj = reinterpret_cast<Mle *>(context);
    obj->HandleUdpReceive(*static_cast<Message *>(message), *static_cast<const Ip6::MessageInfo *>(messageInfo));
}

void Mle::HandleUdpReceive(Message &message, const Ip6::MessageInfo &messageInfo)
{
    Header header;
    uint32_t keySequence;
    const uint8_t *mleKey;
    uint8_t keyid;
    uint32_t frameCounter;
    uint8_t messageTag[4];
    uint8_t messageTagLength;
    uint8_t nonce[13];
    Mac::ExtAddress macAddr;
    Crypto::AesEcb aesEcb;
    Crypto::AesCcm aesCcm;
    uint16_t mleOffset;
    uint8_t buf[64];
    int length;
    uint8_t tag[4];
    uint8_t tagLength;
    uint8_t command;
    Neighbor *neighbor;

    message.Read(message.GetOffset(), sizeof(header), &header);
    VerifyOrExit(header.IsValid(),);

    if (header.IsKeyIdMode1())
    {
        keyid = header.GetKeyId();

        if (keyid == (mKeyManager->GetCurrentKeySequence() & 0x7f))
        {
            keySequence = mKeyManager->GetCurrentKeySequence();
            mleKey = mKeyManager->GetCurrentMleKey();
        }
        else if (mKeyManager->IsPreviousKeyValid() &&
                 keyid == (mKeyManager->GetPreviousKeySequence() & 0x7f))
        {
            keySequence = mKeyManager->GetPreviousKeySequence();
            mleKey = mKeyManager->GetPreviousMleKey();
        }
        else
        {
            keySequence = (mKeyManager->GetCurrentKeySequence() & ~0x7f) | keyid;

            if (keySequence < mKeyManager->GetCurrentKeySequence())
            {
                keySequence += 128;
            }

            mleKey = mKeyManager->GetTemporaryMleKey(keySequence);
        }
    }
    else
    {
        keySequence = header.GetKeyId();

        if (keySequence == mKeyManager->GetCurrentKeySequence())
        {
            mleKey = mKeyManager->GetCurrentMleKey();
        }
        else if (mKeyManager->IsPreviousKeyValid() &&
                 keySequence == mKeyManager->GetPreviousKeySequence())
        {
            mleKey = mKeyManager->GetPreviousMleKey();
        }
        else
        {
            mleKey = mKeyManager->GetTemporaryMleKey(keySequence);
        }
    }

    message.MoveOffset(header.GetLength() - 1);

    frameCounter = header.GetFrameCounter();

    messageTagLength = message.Read(message.GetLength() - sizeof(messageTag), sizeof(messageTag), messageTag);
    VerifyOrExit(messageTagLength == sizeof(messageTag), ;);
    SuccessOrExit(message.SetLength(message.GetLength() - sizeof(messageTag)));

    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;
    GenerateNonce(macAddr, frameCounter, Mac::Frame::kSecEncMic32, nonce);

    aesEcb.SetKey(mleKey, 16);
    aesCcm.Init(aesEcb, sizeof(messageInfo.GetPeerAddr()) + sizeof(messageInfo.GetSockAddr()) + header.GetHeaderLength(),
                message.GetLength() - message.GetOffset(), sizeof(messageTag), nonce, sizeof(nonce));
    aesCcm.Header(&messageInfo.GetPeerAddr(), sizeof(messageInfo.GetPeerAddr()));
    aesCcm.Header(&messageInfo.GetSockAddr(), sizeof(messageInfo.GetSockAddr()));
    aesCcm.Header(header.GetBytes() + 1, header.GetHeaderLength());

    mleOffset = message.GetOffset();

    while (message.GetOffset() < message.GetLength())
    {
        length = message.Read(message.GetOffset(), sizeof(buf), buf);
        aesCcm.Payload(buf, buf, length, false);
        message.Write(message.GetOffset(), length, buf);
        message.MoveOffset(length);
    }

    tagLength = sizeof(tag);
    aesCcm.Finalize(tag, &tagLength);
    VerifyOrExit(messageTagLength == tagLength && memcmp(messageTag, tag, tagLength) == 0, ;);

    if (keySequence > mKeyManager->GetCurrentKeySequence())
    {
        mKeyManager->SetCurrentKeySequence(keySequence);
    }

    message.SetOffset(mleOffset);

    message.Read(message.GetOffset(), sizeof(command), &command);
    message.MoveOffset(sizeof(command));

    switch (mDeviceState)
    {
    case kDeviceStateDetached:
    case kDeviceStateChild:
        neighbor = GetNeighbor(macAddr);
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        if (command == Header::kCommandChildIdResponse)
        {
            neighbor = GetNeighbor(macAddr);
        }
        else
        {
            neighbor = mMleRouter->GetNeighbor(macAddr);
        }

        break;

    default:
        neighbor = NULL;
        break;
    }

    if (neighbor != NULL && neighbor->mState == Neighbor::kStateValid)
    {
        if (keySequence == mKeyManager->GetCurrentKeySequence())
            VerifyOrExit(neighbor->mPreviousKey == true || frameCounter >= neighbor->mValid.mMleFrameCounter,
                         dprintf("mle frame counter reject 1\n"));
        else if (keySequence == mKeyManager->GetPreviousKeySequence())
            VerifyOrExit(neighbor->mPreviousKey == true && frameCounter >= neighbor->mValid.mMleFrameCounter,
                         dprintf("mle frame counter reject 2\n"));
        else
        {
            assert(false);
        }

        neighbor->mValid.mMleFrameCounter = frameCounter + 1;
    }
    else
    {
        VerifyOrExit(command == Header::kCommandLinkRequest ||
                     command == Header::kCommandLinkAccept ||
                     command == Header::kCommandLinkAcceptAndRequest ||
                     command == Header::kCommandAdvertisement ||
                     command == Header::kCommandParentRequest ||
                     command == Header::kCommandParentResponse ||
                     command == Header::kCommandChildIdRequest ||
                     command == Header::kCommandChildUpdateRequest, dprintf("mle sequence unknown! %d\n", command));
    }

    switch (command)
    {
    case Header::kCommandLinkRequest:
        mMleRouter->HandleLinkRequest(message, messageInfo);
        break;

    case Header::kCommandLinkAccept:
        mMleRouter->HandleLinkAccept(message, messageInfo, keySequence);
        break;

    case Header::kCommandLinkAcceptAndRequest:
        mMleRouter->HandleLinkAcceptAndRequest(message, messageInfo, keySequence);
        break;

    case Header::kCommandLinkReject:
        mMleRouter->HandleLinkReject(message, messageInfo);
        break;

    case Header::kCommandAdvertisement:
        HandleAdvertisement(message, messageInfo);
        break;

    case Header::kCommandDataRequest:
        HandleDataRequest(message, messageInfo);
        break;

    case Header::kCommandDataResponse:
        HandleDataResponse(message, messageInfo);
        break;

    case Header::kCommandParentRequest:
        mMleRouter->HandleParentRequest(message, messageInfo);
        break;

    case Header::kCommandParentResponse:
        HandleParentResponse(message, messageInfo, keySequence);
        break;

    case Header::kCommandChildIdRequest:
        mMleRouter->HandleChildIdRequest(message, messageInfo, keySequence);
        break;

    case Header::kCommandChildIdResponse:
        HandleChildIdResponse(message, messageInfo);
        break;

    case Header::kCommandChildUpdateRequest:
        mMleRouter->HandleChildUpdateRequest(message, messageInfo);
        break;

    case Header::kCommandChildUpdateResponse:
        HandleChildUpdateResponse(message, messageInfo);
        break;
    }

exit:
    {}
}

ThreadError Mle::HandleAdvertisement(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Mac::ExtAddress macAddr;
    bool isNeighbor;
    Neighbor *neighbor;
    LeaderDataTlv leaderData;
    uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};

    if (mDeviceState != kDeviceStateDetached)
    {
        SuccessOrExit(error = mMleRouter->HandleAdvertisement(message, messageInfo));
    }

    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    isNeighbor = false;

    switch (mDeviceState)
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        break;

    case kDeviceStateChild:
        if (memcmp(&mParent.mMacAddr, &macAddr, sizeof(mParent.mMacAddr)))
        {
            break;
        }

        isNeighbor = true;
        mParent.mLastHeard = mParentRequestTimer.GetNow();
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        if ((neighbor = mMleRouter->GetNeighbor(macAddr)) != NULL &&
            neighbor->mState == Neighbor::kStateValid)
        {
            isNeighbor = true;
        }

        break;
    }

    if (isNeighbor)
    {
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
        VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);

        if (static_cast<int8_t>(leaderData.GetDataVersion() - mNetworkData->GetVersion()) > 0)
        {
            SendDataRequest(messageInfo.GetPeerAddr(), tlvs, sizeof(tlvs));
        }
    }

exit:
    return error;
}

ThreadError Mle::HandleDataRequest(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    TlvRequestTlv tlvRequest;

    dprintf("Received Data Request\n");

    // TLV Request
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlvRequest), tlvRequest));
    VerifyOrExit(tlvRequest.IsValid(), error = kThreadError_Parse);

    SendDataResponse(messageInfo.GetPeerAddr(), tlvRequest.GetTlvs(), tlvRequest.GetLength());

exit:
    return error;
}

ThreadError Mle::HandleDataResponse(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    LeaderDataTlv leaderData;
    NetworkDataTlv networkData;
    int8_t diff;

    dprintf("Received Data Response\n");

    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kNetworkData, sizeof(networkData), networkData));
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
    VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);

    diff = leaderData.GetDataVersion() - mNetworkData->GetVersion();
    VerifyOrExit(diff > 0, ;);

    SuccessOrExit(error = mNetworkData->SetNetworkData(leaderData.GetDataVersion(),
                                                       leaderData.GetStableDataVersion(),
                                                       (mDeviceMode & kModeFullNetworkData) == 0,
                                                       networkData.GetNetworkData(), networkData.GetLength()));

exit:
    return error;
}

uint8_t Mle::LinkMarginToQuality(uint8_t linkMargin)
{
    if (linkMargin > 20)
    {
        return 3;
    }
    else if (linkMargin > 10)
    {
        return 2;
    }
    else if (linkMargin > 2)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

ThreadError Mle::HandleParentResponse(const Message &message, const Ip6::MessageInfo &messageInfo,
                                      uint32_t keySequence)
{
    ThreadError error = kThreadError_None;
    ResponseTlv response;
    SourceAddressTlv sourceAddress;
    LeaderDataTlv leaderData;
    uint32_t peerPartitionId;
    LinkMarginTlv linkMarginTlv;
    uint8_t linkMargin;
    uint8_t link_quality;
    ConnectivityTlv connectivity;
    uint32_t connectivity_metric;
    LinkFrameCounterTlv linkFrameCounter;
    MleFrameCounterTlv mleFrameCounter;
    ChallengeTlv challenge;
    int8_t diff;

    dprintf("Received Parent Response\n");

    // Response
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
    VerifyOrExit(response.IsValid() &&
                 memcmp(response.GetResponse(), mParentRequest.mChallenge, response.GetLength()) == 0,
                 error = kThreadError_Parse);

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(sourceAddress), sourceAddress));
    VerifyOrExit(sourceAddress.IsValid(), error = kThreadError_Parse);

    // Leader Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
    VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);

    // Weight
    VerifyOrExit(leaderData.GetWeighting() >= mMleRouter->GetLeaderWeight(), ;);

    // Partition ID
    peerPartitionId = leaderData.GetPartitionId();

    if (mDeviceState != kDeviceStateDetached)
    {
        switch (mParentRequestMode)
        {
        case kMleAttachAnyPartition:
            break;

        case kMleAttachSamePartition:
            if (peerPartitionId != mLeaderData.GetPartitionId())
            {
                ExitNow();
            }

            break;

        case kMleAttachBetterPartition:
            dprintf("partition info  %d %d %d %d\n",
                    leaderData.GetWeighting(), peerPartitionId,
                    mLeaderData.GetWeighting(), mLeaderData.GetPartitionId());

            if ((leaderData.GetWeighting() < mLeaderData.GetWeighting()) ||
                (leaderData.GetWeighting() == mLeaderData.GetWeighting() &&
                 peerPartitionId <= mLeaderData.GetPartitionId()))
            {
                ExitNow(dprintf("ignore parent response\n"));
            }

            break;
        }
    }

    // Link Quality
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkMargin, sizeof(linkMarginTlv), linkMarginTlv));
    VerifyOrExit(linkMarginTlv.IsValid(), error = kThreadError_Parse);

    linkMargin = reinterpret_cast<const ThreadMessageInfo *>(messageInfo.mLinkInfo)->mLinkMargin;

    if (linkMargin > linkMarginTlv.GetLinkMargin())
    {
        linkMargin = linkMarginTlv.GetLinkMargin();
    }

    link_quality = LinkMarginToQuality(linkMargin);

    VerifyOrExit(mParentRequestState != kParentRequestRouter || link_quality == 3, ;);

    // Connectivity
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kConnectivity, sizeof(connectivity), connectivity));
    VerifyOrExit(connectivity.IsValid(), error = kThreadError_Parse);

    if (peerPartitionId == mLeaderData.GetPartitionId())
    {
        diff = connectivity.GetRouterIdSequence() - mMleRouter->GetRouterIdSequence();
        VerifyOrExit(diff > 0 || (diff == 0 && mMleRouter->GetLeaderAge() < mMleRouter->GetNetworkIdTimeout()), ;);
    }

    connectivity_metric =
        (static_cast<uint32_t>(link_quality) << 24) |
        (static_cast<uint32_t>(connectivity.GetLinkQuality3()) << 16) |
        (static_cast<uint32_t>(connectivity.GetLinkQuality2()) << 8) |
        (static_cast<uint32_t>(connectivity.GetLinkQuality1()));

    if (mParent.mState == Neighbor::kStateValid)
    {
        VerifyOrExit(connectivity_metric > mParentConnectivity, ;);
    }

    // Link Frame Counter
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkFrameCounter, sizeof(linkFrameCounter),
                                      linkFrameCounter));
    VerifyOrExit(linkFrameCounter.IsValid(), error = kThreadError_Parse);

    // Mle Frame Counter
    if (Tlv::GetTlv(message, Tlv::kMleFrameCounter, sizeof(mleFrameCounter), mleFrameCounter) ==
        kThreadError_None)
    {
        VerifyOrExit(mleFrameCounter.IsValid(), ;);
    }
    else
    {
        mleFrameCounter.SetFrameCounter(linkFrameCounter.GetFrameCounter());
    }

    // Challenge
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
    VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);
    memcpy(mChildIdRequest.mChallenge, challenge.GetChallenge(), challenge.GetLength());
    mChildIdRequest.mChallengeLength = challenge.GetLength();

    memcpy(&mParent.mMacAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(mParent.mMacAddr));
    mParent.mMacAddr.mBytes[0] ^= 0x2;
    mParent.mValid.mRloc16 = sourceAddress.GetRloc16();
    mParent.mValid.mLinkFrameCounter = linkFrameCounter.GetFrameCounter();
    mParent.mValid.mMleFrameCounter = mleFrameCounter.GetFrameCounter();
    mParent.mMode = kModeFFD | kModeRxOnWhenIdle | kModeFullNetworkData;
    mParent.mState = Neighbor::kStateValid;
    assert(keySequence == mKeyManager->GetCurrentKeySequence() ||
           keySequence == mKeyManager->GetPreviousKeySequence());
    mParent.mPreviousKey = keySequence == mKeyManager->GetPreviousKeySequence();
    mParentConnectivity = connectivity_metric;

exit:
    return error;
}

ThreadError Mle::HandleChildIdResponse(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    LeaderDataTlv leaderData;
    SourceAddressTlv sourceAddress;
    Address16Tlv shortAddress;
    NetworkDataTlv networkData;
    RouteTlv route;
    uint8_t numRouters;

    dprintf("Received Child ID Response\n");

    VerifyOrExit(mParentRequestState == kChildIdRequest, ;);

    // Leader Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
    VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(sourceAddress), sourceAddress));
    VerifyOrExit(sourceAddress.IsValid(), error = kThreadError_Parse);

    // ShortAddress
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kAddress16, sizeof(shortAddress), shortAddress));
    VerifyOrExit(shortAddress.IsValid(), error = kThreadError_Parse);

    // Network Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kNetworkData, sizeof(networkData), networkData));
    SuccessOrExit(error = mNetworkData->SetNetworkData(leaderData.GetDataVersion(),
                                                       leaderData.GetStableDataVersion(),
                                                       (mDeviceMode & kModeFullNetworkData) == 0,
                                                       networkData.GetNetworkData(), networkData.GetLength()));

    // Parent Attach Success
    mParentRequestTimer.Stop();

    mLeaderData.SetPartitionId(leaderData.GetPartitionId());
    mLeaderData.SetWeighting(leaderData.GetWeighting());
    mLeaderData.SetRouterId(leaderData.GetRouterId());

    if ((mDeviceMode & kModeRxOnWhenIdle) == 0)
    {
        mMesh->SetPollPeriod((mTimeout / 2) * 1000U);
        mMesh->SetRxOnWhenIdle(false);
    }
    else
    {
        mMesh->SetRxOnWhenIdle(true);
    }

    mParent.mValid.mRloc16 = sourceAddress.GetRloc16();
    SuccessOrExit(error = SetStateChild(shortAddress.GetRloc16()));

    // Route
    if (Tlv::GetTlv(message, Tlv::kRoute, sizeof(route), route) == kThreadError_None)
    {
        numRouters = 0;

        for (int i = 0; i < kMaxRouterId; i++)
        {
            if (route.IsRouterIdSet(i))
            {
                numRouters++;
            }
        }

        if (numRouters < mMleRouter->GetRouterUpgradeThreshold())
        {
            mMleRouter->BecomeRouter();
        }
    }

exit:
    return error;
}

ThreadError Mle::HandleChildUpdateResponse(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    StatusTlv status;
    ModeTlv mode;
    ResponseTlv response;
    LeaderDataTlv leaderData;
    SourceAddressTlv sourceAddress;
    TimeoutTlv timeout;
    uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};

    dprintf("Received Child Update Response\n");

    // Status
    if (Tlv::GetTlv(message, Tlv::kStatus, sizeof(status), status) == kThreadError_None)
    {
        BecomeDetached();
        ExitNow();
    }

    // Mode
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kMode, sizeof(mode), mode));
    VerifyOrExit(mode.IsValid(), error = kThreadError_Parse);
    VerifyOrExit(mode.GetMode() == mDeviceMode, error = kThreadError_Drop);

    switch (mDeviceState)
    {
    case kDeviceStateDetached:
        // Response
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
        VerifyOrExit(response.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(memcmp(response.GetResponse(), mParentRequest.mChallenge,
                            sizeof(mParentRequest.mChallenge)) == 0,
                     error = kThreadError_Drop);

        SetStateChild(GetRloc16());
        break;

    case kDeviceStateChild:
        // Leader Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
        VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);

        if (static_cast<int8_t>(leaderData.GetDataVersion() - mNetworkData->GetVersion()) > 0)
        {
            SendDataRequest(messageInfo.GetPeerAddr(), tlvs, sizeof(tlvs));
        }

        // Source Address
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(sourceAddress), sourceAddress));
        VerifyOrExit(sourceAddress.IsValid(), error = kThreadError_Parse);

        if (GetRouterId(sourceAddress.GetRloc16()) != GetRouterId(GetRloc16()))
        {
            BecomeDetached();
            ExitNow();
        }

        // Timeout
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTimeout, sizeof(timeout), timeout));
        VerifyOrExit(timeout.IsValid(), error = kThreadError_Parse);

        mTimeout = timeout.GetTimeout();

        if ((mode.GetMode() & kModeRxOnWhenIdle) == 0)
        {
            mMesh->SetPollPeriod((mTimeout / 2) * 1000U);
            mMesh->SetRxOnWhenIdle(false);
        }
        else
        {
            mMesh->SetRxOnWhenIdle(true);
        }

        break;

    default:
        assert(false);
        break;
    }

exit:
    return error;
}

Neighbor *Mle::GetNeighbor(uint16_t address)
{
    return (mParent.mState == Neighbor::kStateValid && mParent.mValid.mRloc16 == address) ? &mParent : NULL;
}

Neighbor *Mle::GetNeighbor(const Mac::ExtAddress &address)
{
    return (mParent.mState == Neighbor::kStateValid &&
            memcmp(&mParent.mMacAddr, &address, sizeof(mParent.mMacAddr)) == 0) ? &mParent : NULL;
}

Neighbor *Mle::GetNeighbor(const Mac::Address &address)
{
    Neighbor *neighbor = NULL;

    switch (address.mLength)
    {
    case 2:
        neighbor = GetNeighbor(address.mShortAddress);
        break;

    case 8:
        neighbor = GetNeighbor(address.mExtAddress);
        break;
    }

    return neighbor;
}

Neighbor *Mle::GetNeighbor(const Ip6::Address &address)
{
    return NULL;
}

uint16_t Mle::GetNextHop(uint16_t destination) const
{
    return (mParent.mState == Neighbor::kStateValid) ? mParent.mValid.mRloc16 : Mac::kShortAddrInvalid;
}

bool Mle::IsRoutingLocator(const Ip6::Address &address) const
{
    return memcmp(&mMeshLocal16, &address, 14) == 0;
}

Router *Mle::GetParent()
{
    return &mParent;
}

ThreadError Mle::CheckReachability(Mac::ShortAddress meshsrc, Mac::ShortAddress meshdst, Ip6::Header &ip6Header)
{
    ThreadError error = kThreadError_Drop;
    Ip6::Address dst;

    if (meshdst != GetRloc16())
    {
        ExitNow(error = kThreadError_None);
    }

    if (mNetif->IsUnicastAddress(ip6Header.GetDestination()))
    {
        ExitNow(error = kThreadError_None);
    }

    memcpy(&dst, GetMeshLocal16(), 14);
    dst.m16[7] = HostSwap16(meshsrc);
    Ip6::Icmp::SendError(dst, Ip6::IcmpHeader::kTypeDstUnreach, Ip6::IcmpHeader::kCodeDstUnreachNoRoute, ip6Header);

exit:
    return error;
}

ThreadError Mle::HandleNetworkDataUpdate()
{
    if (mDeviceMode & kModeFFD)
    {
        mMleRouter->HandleNetworkDataUpdateRouter();
    }

    switch (mDeviceState)
    {
    case kDeviceStateChild:
        SendChildUpdateRequest();
        break;

    default:
        break;
    }

    return kThreadError_None;
}

}  // namespace Mle
}  // namespace Thread
