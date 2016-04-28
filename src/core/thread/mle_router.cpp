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
 *   This file implements MLE functionality required for the Thread Router and Leader roles.
 */

#include <assert.h>

#include <common/code_utils.hpp>
#include <common/encoding.hpp>
#include <mac/mac_frame.hpp>
#include <net/icmp6.hpp>
#include <platform/random.h>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>
#include <thread/thread_tlvs.hpp>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {
namespace Mle {

#define LQI_TO_COST(x) (kLqiToCost[(x)])

static const uint8_t kLqiToCost[] =
{
    16, 6, 2, 1,
};

MleRouter::MleRouter(ThreadNetif &netif):
    Mle(netif),
    mAdvertiseTimer(&HandleAdvertiseTimer, this),
    mStateUpdateTimer(&HandleStateUpdateTimer, this),
    mAddressSolicit("a/as", &HandleAddressSolicit, this),
    mAddressRelease("a/ar", &HandleAddressRelease, this)
{
    mNextChildId = 1;
    mRouterIdSequence = 0;
    memset(mChildren, 0, sizeof(mChildren));
    memset(mRouters, 0, sizeof(mRouters));
    mCoapServer = netif.GetCoapServer();
    mCoapMessageId = otRandomGet();
}

int MleRouter::AllocateRouterId()
{
    int rval = -1;

    // count available router ids
    uint8_t numAvailable = 0;
    uint8_t numAllocated = 0;

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mAllocated)
        {
            numAllocated++;
        }
        else if (mRouters[i].mReclaimDelay == false)
        {
            numAvailable++;
        }
    }

    VerifyOrExit(numAllocated < kMaxRouters && numAvailable > 0, rval = -1);

    // choose available router id at random
    uint8_t freeBit;
    // freeBit = otRandomGet() % numAvailable;
    freeBit = 0;

    // allocate router id
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mAllocated || mRouters[i].mReclaimDelay)
        {
            continue;
        }

        if (freeBit == 0)
        {
            rval = AllocateRouterId(i);
            ExitNow();
        }

        freeBit--;
    }

exit:
    return rval;
}

int MleRouter::AllocateRouterId(uint8_t routerId)
{
    int rval = -1;

    VerifyOrExit(!mRouters[routerId].mAllocated, rval = -1);

    // init router state
    mRouters[routerId].mAllocated = true;
    mRouters[routerId].mLastHeard = Timer::GetNow();
    memset(&mRouters[routerId].mMacAddr, 0, sizeof(mRouters[routerId].mMacAddr));

    // bump sequence number
    mRouterIdSequence++;
    mRouterIdSequenceLastUpdated = Timer::GetNow();
    rval = routerId;

    dprintf("add router id %d\n", routerId);

exit:
    return rval;
}

ThreadError MleRouter::ReleaseRouterId(uint8_t routerId)
{
    dprintf("delete router id %d\n", routerId);
    mRouters[routerId].mAllocated = false;
    mRouters[routerId].mReclaimDelay = true;
    mRouters[routerId].mState = Neighbor::kStateInvalid;
    mRouters[routerId].mNextHop = kMaxRouterId;
    mRouterIdSequence++;
    mRouterIdSequenceLastUpdated = Timer::GetNow();
    mAddressResolver->Remove(routerId);
    mNetworkData->RemoveBorderRouter(GetRloc16(routerId));
    ResetAdvertiseInterval();
    return kThreadError_None;
}

uint32_t MleRouter::GetLeaderAge() const
{
    return (Timer::GetNow() - mRouterIdSequenceLastUpdated) / 1000;
}

ThreadError MleRouter::BecomeRouter()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mDeviceState == kDeviceStateDetached || mDeviceState == kDeviceStateChild,
                 error = kThreadError_Busy);
    VerifyOrExit(mDeviceMode & kModeFFD, ;);

    for (int i = 0; i < kMaxRouterId; i++)
    {
        mRouters[i].mAllocated = false;
        mRouters[i].mReclaimDelay = false;
        mRouters[i].mState = Neighbor::kStateInvalid;
        mRouters[i].mNextHop = kMaxRouterId;
    }

    mSocket.Open(&HandleUdpReceive, this);
    mAdvertiseTimer.Stop();
    mAddressResolver->Clear();

    switch (mDeviceState)
    {
    case kDeviceStateDetached:
        SuccessOrExit(error = SendLinkRequest(NULL));
        mStateUpdateTimer.Start(1000);
        break;

    case kDeviceStateChild:
        SuccessOrExit(error = SendAddressSolicit());
        break;

    default:
        assert(false);
        break;
    }

exit:
    return error;
}

ThreadError MleRouter::BecomeLeader()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mDeviceState != kDeviceStateDisabled && mDeviceState != kDeviceStateLeader,
                 error = kThreadError_Busy);

    for (int i = 0; i < kMaxRouterId; i++)
    {
        mRouters[i].mAllocated = false;
        mRouters[i].mReclaimDelay = false;
        mRouters[i].mState = Neighbor::kStateInvalid;
        mRouters[i].mNextHop = kMaxRouterId;
    }

    mSocket.Open(&HandleUdpReceive, this);
    mAdvertiseTimer.Stop();
    ResetAdvertiseInterval();
    mStateUpdateTimer.Start(1000);
    mAddressResolver->Clear();

    mRouterId = (mPreviousRouterId != kMaxRouterId) ? AllocateRouterId(mPreviousRouterId) : AllocateRouterId();
    VerifyOrExit(mRouterId >= 0, error = kThreadError_NoBufs);

    memcpy(&mRouters[mRouterId].mMacAddr, mMesh->GetExtAddress(), sizeof(mRouters[mRouterId].mMacAddr));

    mLeaderData.SetPartitionId(otRandomGet());
    mLeaderData.SetWeighting(mLeaderWeight);
    mLeaderData.SetRouterId(mRouterId);

    mNetworkData->Reset();

    SuccessOrExit(error = SetStateLeader(mRouterId << 10));

exit:
    return error;
}

ThreadError MleRouter::HandleDetachStart()
{
    ThreadError error = kThreadError_None;

    for (int i = 0; i < kMaxRouterId; i++)
    {
        mRouters[i].mState = Neighbor::kStateInvalid;
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        mChildren[i].mState = Neighbor::kStateInvalid;
    }

    mAdvertiseTimer.Stop();
    mStateUpdateTimer.Stop();
    mNetworkData->Stop();
    mNetif->UnsubscribeAllRoutersMulticast();

    return error;
}

ThreadError MleRouter::HandleChildStart(otMleAttachFilter filter)
{
    uint32_t advertiseDelay;

    mRouterIdSequenceLastUpdated = Timer::GetNow();

    mAdvertiseTimer.Stop();
    mStateUpdateTimer.Start(1000);
    mNetworkData->Stop();

    switch (filter)
    {
    case kMleAttachAnyPartition:
        break;

    case kMleAttachSamePartition:
        SendAddressRelease();
        break;

    case kMleAttachBetterPartition:
        // BecomeRouter();
        break;
    }

    if (mDeviceMode & kModeFFD)
    {
        advertiseDelay = (kReedAdvertiseInterval + (otRandomGet() % kReedAdvertiseJitter)) * 1000U;
        mAdvertiseTimer.Start(advertiseDelay);
        mNetif->SubscribeAllRoutersMulticast();
    }
    else
    {
        mNetif->UnsubscribeAllRoutersMulticast();
    }

    return kThreadError_None;
}

ThreadError MleRouter::SetStateRouter(uint16_t rloc16)
{
    SetRloc16(rloc16);
    mDeviceState = kDeviceStateRouter;
    mParentRequestState = kParentIdle;
    mParentRequestTimer.Stop();

    mNetif->SubscribeAllRoutersMulticast();
    mRouters[mRouterId].mNextHop = mRouterId;
    mNetworkData->Stop();
    mStateUpdateTimer.Start(1000);

    dprintf("Mode -> Router\n");
    return kThreadError_None;
}

ThreadError MleRouter::SetStateLeader(uint16_t rloc16)
{
    SetRloc16(rloc16);
    mDeviceState = kDeviceStateLeader;
    mParentRequestState = kParentIdle;
    mParentRequestTimer.Stop();

    mNetif->SubscribeAllRoutersMulticast();
    mRouters[mRouterId].mNextHop = mRouterId;
    mRouters[mRouterId].mLastHeard = Timer::GetNow();

    mNetworkData->Start();
    mCoapServer->AddResource(mAddressSolicit);
    mCoapServer->AddResource(mAddressRelease);

    dprintf("Mode -> Leader %d\n", mLeaderData.GetPartitionId());
    return kThreadError_None;
}

uint8_t MleRouter::GetNetworkIdTimeout() const
{
    return mNetworkIdTimeout;
}

ThreadError MleRouter::SetNetworkIdTimeout(uint8_t timeout)
{
    mNetworkIdTimeout = timeout;
    return kThreadError_None;
}

uint8_t MleRouter::GetRouterUpgradeThreshold() const
{
    return mRouterUpgradeThreshold;
}

ThreadError MleRouter::SetRouterUpgradeThreshold(uint8_t threshold)
{
    mRouterUpgradeThreshold = threshold;
    return kThreadError_None;
}

void MleRouter::HandleAdvertiseTimer(void *context)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleAdvertiseTimer();
}

void MleRouter::HandleAdvertiseTimer()
{
    uint32_t advertiseDelay;

    if ((mDeviceMode & kModeFFD) == 0)
    {
        return;
    }

    SendAdvertisement();

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        advertiseDelay = (kReedAdvertiseInterval + (otRandomGet() % kReedAdvertiseJitter)) * 1000U;
        mAdvertiseTimer.Start(advertiseDelay);
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        mAdvertiseInterval *= 2;

        if (mAdvertiseInterval > kAdvertiseIntervalMax)
        {
            mAdvertiseInterval = kAdvertiseIntervalMax;
        }

        advertiseDelay = (mAdvertiseInterval * 1000U) / 2;
        advertiseDelay += otRandomGet() % (advertiseDelay);
        mAdvertiseTimer.Start(advertiseDelay);
        break;
    }
}

ThreadError MleRouter::ResetAdvertiseInterval()
{
    uint32_t advertiseDelay;

    VerifyOrExit(mAdvertiseInterval != kAdvertiseIntervalMin || !mAdvertiseTimer.IsRunning(), ;);

    mAdvertiseInterval = kAdvertiseIntervalMin;

    advertiseDelay = (mAdvertiseInterval * 1000U) / 2;
    advertiseDelay += otRandomGet() % advertiseDelay;
    mAdvertiseTimer.Start(advertiseDelay);

    dprintf("reset advertise interval\n");

exit:
    return kThreadError_None;
}

ThreadError MleRouter::SendAdvertisement()
{
    ThreadError error = kThreadError_None;
    Ip6::Address destination;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandAdvertisement));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendLeaderData(*message));

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        SuccessOrExit(error = AppendRoute(*message));
        break;
    }

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xff02);
    destination.m16[7] = HostSwap16(0x0001);
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent advertisement\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::SendLinkRequest(Neighbor *neighbor)
{
    static const uint8_t detachedTlvs[] = {Tlv::kNetworkData, Tlv::kAddress16, Tlv::kRoute};
    static const uint8_t routerTlvs[] = {Tlv::kLinkMargin};
    ThreadError error = kThreadError_None;
    Message *message;
    Ip6::Address destination;

    memset(&destination, 0, sizeof(destination));

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandLinkRequest));
    SuccessOrExit(error = AppendVersion(*message));

    switch (mDeviceState)
    {
    case kDeviceStateDisabled:
        assert(false);
        break;

    case kDeviceStateDetached:
        SuccessOrExit(error = AppendTlvRequest(*message, detachedTlvs, sizeof(detachedTlvs)));
        break;

    case kDeviceStateChild:
        SuccessOrExit(error = AppendSourceAddress(*message));
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        SuccessOrExit(error = AppendTlvRequest(*message, routerTlvs, sizeof(routerTlvs)));
        SuccessOrExit(error = AppendSourceAddress(*message));
        SuccessOrExit(error = AppendLeaderData(*message));
        break;
    }

    if (neighbor == NULL)
    {
        for (uint8_t i = 0; i < sizeof(mChallenge); i++)
        {
            mChallenge[i] = otRandomGet();
        }

        SuccessOrExit(error = AppendChallenge(*message, mChallenge, sizeof(mChallenge)));
        destination.m8[0] = 0xff;
        destination.m8[1] = 0x02;
        destination.m8[15] = 2;
    }
    else
    {
        for (uint8_t i = 0; i < sizeof(neighbor->mPending.mChallenge); i++)
        {
            neighbor->mPending.mChallenge[i] = otRandomGet();
        }

        SuccessOrExit(error = AppendChallenge(*message, mChallenge, sizeof(mChallenge)));
        destination.m16[0] = HostSwap16(0xfe80);
        memcpy(destination.m8 + 8, &neighbor->mMacAddr, sizeof(neighbor->mMacAddr));
        destination.m8[8] ^= 0x2;
    }

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent link request\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::HandleLinkRequest(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Neighbor *neighbor = NULL;
    Mac::ExtAddress macAddr;
    ChallengeTlv challenge;
    VersionTlv version;
    LeaderDataTlv leaderData;
    SourceAddressTlv sourceAddress;
    TlvRequestTlv tlvRequest;
    uint16_t rloc16;

    dprintf("Received link request\n");

    VerifyOrExit(GetDeviceState() == kDeviceStateRouter ||
                 GetDeviceState() == kDeviceStateLeader, ;);

    VerifyOrExit(mParentRequestState == kParentIdle, ;);

    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    // Challenge
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
    VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);

    // Version
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kVersion, sizeof(version), version));
    VerifyOrExit(version.IsValid() && version.GetVersion() == kVersion, error = kThreadError_Parse);

    // Leader Data
    if (Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData) == kThreadError_None)
    {
        VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(leaderData.GetPartitionId() == mLeaderData.GetPartitionId(), ;);
    }

    // Source Address
    if (Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(sourceAddress), sourceAddress) == kThreadError_None)
    {
        VerifyOrExit(sourceAddress.IsValid(), error = kThreadError_Parse);

        rloc16 = sourceAddress.GetRloc16();

        if ((neighbor = GetNeighbor(macAddr)) != NULL && neighbor->mValid.mRloc16 != rloc16)
        {
            // remove stale neighbors
            neighbor->mState = Neighbor::kStateInvalid;
            neighbor = NULL;
        }

        if (GetChildId(rloc16) == 0)
        {
            // source is a router
            neighbor = &mRouters[GetRouterId(rloc16)];

            if (neighbor->mState != Neighbor::kStateValid)
            {
                memcpy(&neighbor->mMacAddr, &macAddr, sizeof(neighbor->mMacAddr));
                neighbor->mState = Neighbor::kStateLinkRequest;
            }
            else
            {
                VerifyOrExit(memcmp(&neighbor->mMacAddr, &macAddr, sizeof(neighbor->mMacAddr)) == 0, ;);
            }
        }
    }
    else
    {
        // lack of source address indicates router coming out of reset
        VerifyOrExit((neighbor = GetNeighbor(macAddr)) != NULL, error = kThreadError_Drop);
    }

    // TLV Request
    if (Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlvRequest), tlvRequest) == kThreadError_None)
    {
        VerifyOrExit(tlvRequest.IsValid(), error = kThreadError_Parse);
    }
    else
    {
        tlvRequest.SetLength(0);
    }

    SuccessOrExit(error = SendLinkAccept(messageInfo, neighbor, tlvRequest, challenge));

exit:
    return error;
}

ThreadError MleRouter::SendLinkAccept(const Ip6::MessageInfo &messageInfo, Neighbor *neighbor,
                                      const TlvRequestTlv &tlvRequest, const ChallengeTlv &challenge)
{
    ThreadError error = kThreadError_None;
    Message *message;
    Header::Command command;

    command = (neighbor == NULL || neighbor->mState == Neighbor::kStateValid) ?
              Header::kCommandLinkAccept : Header::kCommandLinkAcceptAndRequest;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, command));
    SuccessOrExit(error = AppendVersion(*message));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendResponse(*message, challenge.GetChallenge(), challenge.GetLength()));
    SuccessOrExit(error = AppendLinkFrameCounter(*message));
    SuccessOrExit(error = AppendMleFrameCounter(*message));

    if (neighbor != NULL && GetChildId(neighbor->mValid.mRloc16) == 0)
    {
        SuccessOrExit(error = AppendLeaderData(*message));
    }

    for (uint8_t i = 0; i < tlvRequest.GetLength(); i++)
    {
        switch (tlvRequest.GetTlvs()[i])
        {
        case Tlv::kRoute:
            SuccessOrExit(error = AppendRoute(*message));
            break;

        case Tlv::kAddress16:
            VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
            SuccessOrExit(error = AppendAddress16(*message, neighbor->mValid.mRloc16));
            break;

        case Tlv::kNetworkData:
            VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
            SuccessOrExit(error = AppendNetworkData(*message, (neighbor->mMode & kModeFullNetworkData) == 0));
            break;

        case Tlv::kLinkMargin:
            VerifyOrExit(neighbor != NULL, error = kThreadError_Drop);
            SuccessOrExit(error = AppendLinkMargin(*message, neighbor->mRssi));
            break;

        default:
            ExitNow(error = kThreadError_Drop);
        }
    }

    if (neighbor != NULL && neighbor->mState != Neighbor::kStateValid)
    {
        for (uint8_t i = 0; i < sizeof(neighbor->mPending.mChallenge); i++)
        {
            neighbor->mPending.mChallenge[i] = otRandomGet();
        }

        SuccessOrExit(error = AppendChallenge(*message, neighbor->mPending.mChallenge,
                                              sizeof(neighbor->mPending.mChallenge)));
        neighbor->mState = Neighbor::kStateLinkRequest;
    }

    SuccessOrExit(error = SendMessage(*message, messageInfo.GetPeerAddr()));

    dprintf("Sent link accept\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::HandleLinkAccept(const Message &message, const Ip6::MessageInfo &messageInfo,
                                        uint32_t keySequence)
{
    dprintf("Received link accept\n");
    return HandleLinkAccept(message, messageInfo, keySequence, false);
}

ThreadError MleRouter::HandleLinkAcceptAndRequest(const Message &message, const Ip6::MessageInfo &messageInfo,
                                                  uint32_t keySequence)
{
    dprintf("Received link accept and request\n");
    return HandleLinkAccept(message, messageInfo, keySequence, true);
}

ThreadError MleRouter::HandleLinkAccept(const Message &message, const Ip6::MessageInfo &messageInfo,
                                        uint32_t keySequence, bool request)
{
    ThreadError error = kThreadError_None;
    Neighbor *neighbor = NULL;
    Mac::ExtAddress macAddr;
    VersionTlv version;
    ResponseTlv response;
    SourceAddressTlv sourceAddress;
    LinkFrameCounterTlv linkFrameCounter;
    MleFrameCounterTlv mleFrameCounter;
    uint8_t routerId;
    Address16Tlv address16;
    RouteTlv route;
    LeaderDataTlv leaderData;
    NetworkDataTlv networkData;
    LinkMarginTlv linkMargin;
    ChallengeTlv challenge;
    TlvRequestTlv tlvRequest;

    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    // Version
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kVersion, sizeof(version), version));
    VerifyOrExit(version.IsValid(), error = kThreadError_Parse);

    // Response
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
    VerifyOrExit(response.IsValid(), error = kThreadError_Parse);

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(sourceAddress), sourceAddress));
    VerifyOrExit(sourceAddress.IsValid(), error = kThreadError_Parse);

    // Remove stale neighbors
    if ((neighbor = GetNeighbor(macAddr)) != NULL &&
        neighbor->mValid.mRloc16 != sourceAddress.GetRloc16())
    {
        neighbor->mState = Neighbor::kStateInvalid;
        neighbor = NULL;
    }

    // Link-Layer Frame Counter
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkFrameCounter, sizeof(linkFrameCounter),
                                      linkFrameCounter));
    VerifyOrExit(linkFrameCounter.IsValid(), error = kThreadError_Parse);

    // MLE Frame Counter
    if (Tlv::GetTlv(message, Tlv::kMleFrameCounter, sizeof(mleFrameCounter), mleFrameCounter) ==
        kThreadError_None)
    {
        VerifyOrExit(mleFrameCounter.IsValid(), error = kThreadError_Parse);
    }
    else
    {
        mleFrameCounter.SetFrameCounter(linkFrameCounter.GetFrameCounter());
    }

    routerId = GetRouterId(sourceAddress.GetRloc16());

    if (routerId != mRouterId)
    {
        neighbor = &mRouters[routerId];
    }
    else
    {
        VerifyOrExit((neighbor = FindChild(macAddr)) != NULL, error = kThreadError_Error);
    }

    // verify response
    VerifyOrExit(memcmp(mChallenge, response.GetResponse(), sizeof(mChallenge)) == 0 ||
                 memcmp(neighbor->mPending.mChallenge, response.GetResponse(), sizeof(neighbor->mPending.mChallenge)) == 0,
                 error = kThreadError_Error);

    switch (mDeviceState)
    {
    case kDeviceStateDisabled:
        assert(false);
        break;

    case kDeviceStateDetached:
        // Address16
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kAddress16, sizeof(address16), address16));
        VerifyOrExit(address16.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(GetRloc16() == address16.GetRloc16(), error = kThreadError_Drop);

        // Route
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kRoute, sizeof(route), route));
        VerifyOrExit(route.IsValid(), error = kThreadError_Parse);
        SuccessOrExit(error = ProcessRouteTlv(route));

        // Leader Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
        VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);
        mLeaderData.SetPartitionId(leaderData.GetPartitionId());
        mLeaderData.SetWeighting(leaderData.GetWeighting());
        mLeaderData.SetRouterId(leaderData.GetRouterId());

        // Network Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kNetworkData, sizeof(networkData), networkData));
        mNetworkData->SetNetworkData(leaderData.GetDataVersion(), leaderData.GetStableDataVersion(),
                                     (mDeviceMode & kModeFullNetworkData) == 0,
                                     networkData.GetNetworkData(), networkData.GetLength());

        if (mLeaderData.GetRouterId() == GetRouterId(GetRloc16()))
        {
            SetStateLeader(GetRloc16());
        }
        else
        {
            SetStateRouter(GetRloc16());
        }

        break;

    case kDeviceStateChild:
        mRouters[routerId].mLinkQualityOut = 3;
        mRouters[routerId].mLinkQualityIn = 3;
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        // Leader Data
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
        VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);
        VerifyOrExit(leaderData.GetPartitionId() == mLeaderData.GetPartitionId(), ;);

        // Link Margin
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkMargin, sizeof(linkMargin), linkMargin));
        VerifyOrExit(linkMargin.IsValid(), error = kThreadError_Parse);
        mRouters[routerId].mLinkQualityOut = 3;
        mRouters[routerId].mLinkQualityIn = 3;

        // update routing table
        if (routerId != mRouterId && mRouters[routerId].mNextHop == kMaxRouterId)
        {
            mRouters[routerId].mNextHop = routerId;
            ResetAdvertiseInterval();
        }

        break;
    }

    // finish link synchronization
    memcpy(&neighbor->mMacAddr, &macAddr, sizeof(neighbor->mMacAddr));
    neighbor->mValid.mRloc16 = sourceAddress.GetRloc16();
    neighbor->mValid.mLinkFrameCounter = linkFrameCounter.GetFrameCounter();
    neighbor->mValid.mMleFrameCounter = mleFrameCounter.GetFrameCounter();
    neighbor->mLastHeard = Timer::GetNow();
    neighbor->mMode = kModeFFD | kModeRxOnWhenIdle | kModeFullNetworkData;
    neighbor->mState = Neighbor::kStateValid;
    assert(keySequence == mKeyManager->GetCurrentKeySequence() ||
           keySequence == mKeyManager->GetPreviousKeySequence());
    neighbor->mPreviousKey = keySequence == mKeyManager->GetPreviousKeySequence();

    if (request)
    {
        // Challenge
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
        VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);

        // TLV Request
        if (Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlvRequest), tlvRequest) == kThreadError_None)
        {
            VerifyOrExit(tlvRequest.IsValid(), error = kThreadError_Parse);
        }
        else
        {
            tlvRequest.SetLength(0);
        }

        SuccessOrExit(error = SendLinkAccept(messageInfo, neighbor, tlvRequest, challenge));
    }

exit:
    return error;
}

ThreadError MleRouter::SendLinkReject(const Ip6::Address &destination)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandLinkReject));
    SuccessOrExit(error = AppendStatus(*message, StatusTlv::kError));

    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent link reject\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return error;
}

ThreadError MleRouter::HandleLinkReject(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    Mac::ExtAddress macAddr;

    dprintf("Received link reject\n");

    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    return kThreadError_None;
}

Child *MleRouter::NewChild()
{
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState == Neighbor::kStateInvalid)
        {
            return &mChildren[i];
        }
    }

    return NULL;
}

Child *MleRouter::FindChild(const Mac::ExtAddress &address)
{
    Child *rval = NULL;

    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState != Neighbor::kStateInvalid &&
            memcmp(&mChildren[i].mMacAddr, &address, sizeof(mChildren[i].mMacAddr)) == 0)
        {
            ExitNow(rval = &mChildren[i]);
        }
    }

exit:
    return rval;
}

uint8_t MleRouter::GetLinkCost(uint8_t routerId)
{
    uint8_t rval;

    assert(routerId <= kMaxRouterId);

    VerifyOrExit(routerId != mRouterId &&
                 routerId != kMaxRouterId &&
                 mRouters[routerId].mState == Neighbor::kStateValid,
                 rval = kMaxRouteCost);

    rval = mRouters[routerId].mLinkQualityIn;

    if (rval > mRouters[routerId].mLinkQualityOut)
    {
        rval = mRouters[routerId].mLinkQualityOut;
    }

    rval = LQI_TO_COST(rval);

exit:
    return rval;
}

ThreadError MleRouter::ProcessRouteTlv(const RouteTlv &route)
{
    ThreadError error = kThreadError_None;
    int8_t diff = route.GetRouterIdSequence() - mRouterIdSequence;
    bool old;

    // check for newer route data
    if (diff > 0 || mDeviceState == kDeviceStateDetached)
    {
        mRouterIdSequence = route.GetRouterIdSequence();
        mRouterIdSequenceLastUpdated = Timer::GetNow();

        for (int i = 0; i < kMaxRouterId; i++)
        {
            old = mRouters[i].mAllocated;
            mRouters[i].mAllocated = route.IsRouterIdSet(i);

            if (old && !mRouters[i].mAllocated)
            {
                mRouters[i].mNextHop = kMaxRouterId;
                mAddressResolver->Remove(i);
            }
        }

        if (GetDeviceState() == kDeviceStateRouter && !mRouters[mRouterId].mAllocated)
        {
            BecomeDetached();
            ExitNow(error = kThreadError_NoRoute);
        }

    }

exit:
    return error;
}

ThreadError MleRouter::HandleAdvertisement(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Mac::ExtAddress macAddr;
    SourceAddressTlv sourceAddress;
    LeaderDataTlv leaderData;
    RouteTlv route;
    uint32_t peerParitionId;
    Router *router;
    Neighbor *neighbor;
    uint8_t routerId;
    uint8_t routerCount;

    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    // Source Address
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kSourceAddress, sizeof(sourceAddress), sourceAddress));
    VerifyOrExit(sourceAddress.IsValid(), error = kThreadError_Parse);

    // Remove stale neighbors
    if ((neighbor = GetNeighbor(macAddr)) != NULL &&
        neighbor->mValid.mRloc16 != sourceAddress.GetRloc16())
    {
        neighbor->mState = Neighbor::kStateInvalid;
    }

    // Leader Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData));
    VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);

    dprintf("Received advertisement from %04x\n", sourceAddress.GetRloc16());

    peerParitionId = leaderData.GetPartitionId();

    if (peerParitionId != mLeaderData.GetPartitionId())
    {
        dprintf("different partition! %d %d %d %d\n",
                leaderData.GetWeighting(), peerParitionId,
                mLeaderData.GetWeighting(), mLeaderData.GetPartitionId());

        if ((leaderData.GetWeighting() > mLeaderData.GetWeighting()) ||
            (leaderData.GetWeighting() == mLeaderData.GetWeighting() &&
             peerParitionId > mLeaderData.GetPartitionId()))
        {
            dprintf("trying to migrate\n");
            BecomeChild(kMleAttachBetterPartition);
        }

        ExitNow(error = kThreadError_Drop);
    }
    else if (leaderData.GetRouterId() != GetLeaderId())
    {
        BecomeDetached();
        ExitNow(error = kThreadError_Drop);
    }

    VerifyOrExit(GetChildId(sourceAddress.GetRloc16()) == 0, ;);

    // Route Data
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kRoute, sizeof(route), route));
    VerifyOrExit(route.IsValid(), error = kThreadError_Parse);

    if ((GetDeviceState() == kDeviceStateChild &&
         memcmp(&mParent.mMacAddr, &macAddr, sizeof(mParent.mMacAddr)) == 0) ||
        GetDeviceState() == kDeviceStateRouter || GetDeviceState() == kDeviceStateLeader)
    {
        SuccessOrExit(error = ProcessRouteTlv(route));
    }

    routerId = GetRouterId(sourceAddress.GetRloc16());
    router = NULL;

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        ExitNow();

    case kDeviceStateChild:
        routerCount = 0;

        for (int i = 0; i < kMaxRouterId; i++)
        {
            if (mRouters[i].mAllocated)
            {
                routerCount++;
            }
        }

        if ((mDeviceMode & kModeFFD) && (routerCount < mRouterUpgradeThreshold))
        {
            BecomeRouter();
            ExitNow();
        }

        router = &mParent;

        if (memcmp(&router->mMacAddr, &macAddr, sizeof(router->mMacAddr)) == 0)
        {
            if (router->mValid.mRloc16 != sourceAddress.GetRloc16())
            {
                SetStateDetached();
                ExitNow(error = kThreadError_NoRoute);
            }
        }
        else
        {
            router = &mRouters[routerId];

            if (router->mState != Neighbor::kStateValid)
            {
                memcpy(&router->mMacAddr, &macAddr, sizeof(router->mMacAddr));
                router->mState = Neighbor::kStateLinkRequest;
                router->mPreviousKey = false;
                SendLinkRequest(router);
                ExitNow(error = kThreadError_NoRoute);
            }
        }

        router->mLastHeard = Timer::GetNow();
        router->mLinkQualityIn =
            LinkMarginToQuality(reinterpret_cast<const ThreadMessageInfo *>(messageInfo.mLinkInfo)->mLinkMargin);

        ExitNow();

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        router = &mRouters[routerId];

        // router is not in list, reject
        if (!router->mAllocated)
        {
            ExitNow(error = kThreadError_NoRoute);
        }

        // Send link request if no link to router
        if (router->mState != Neighbor::kStateValid)
        {
            memcpy(&router->mMacAddr, &macAddr, sizeof(router->mMacAddr));
            router->mState = Neighbor::kStateLinkRequest;
            router->mFramePending = false;
            router->mDataRequest = false;
            router->mPreviousKey = false;
            SendLinkRequest(router);
            ExitNow(error = kThreadError_NoRoute);
        }

        router->mLastHeard = Timer::GetNow();
        router->mLinkQualityIn =
            LinkMarginToQuality(reinterpret_cast<const ThreadMessageInfo *>(messageInfo.mLinkInfo)->mLinkMargin);
        break;
    }

    UpdateRoutes(route, routerId);

exit:
    return error;
}

void MleRouter::UpdateRoutes(const RouteTlv &route, uint8_t routerId)
{
    uint8_t curCost;
    uint8_t newCost;
    uint8_t oldNextHop;
    uint8_t cost;
    uint8_t lqi;
    bool update;

    // update routes
    do
    {
        update = false;

        for (int i = 0, routeCount = 0; i < kMaxRouterId; i++)
        {
            if (route.IsRouterIdSet(i) == false)
            {
                continue;
            }

            if (mRouters[i].mAllocated == false)
            {
                routeCount++;
                continue;
            }

            if (i == mRouterId)
            {
                lqi = route.GetLinkQualityIn(routeCount);

                if (mRouters[routerId].mLinkQualityOut != lqi)
                {
                    mRouters[routerId].mLinkQualityOut = lqi;
                    update = true;
                }
            }
            else
            {
                oldNextHop = mRouters[i].mNextHop;

                if (i == routerId)
                {
                    cost = 0;
                }
                else
                {
                    cost = route.GetRouteCost(routeCount);

                    if (cost == 0)
                    {
                        cost = kMaxRouteCost;
                    }
                }

                if (i != routerId && cost == 0 && mRouters[i].mNextHop == routerId)
                {
                    // route nexthop is neighbor, but neighbor no longer has route
                    ResetAdvertiseInterval();
                    mRouters[i].mNextHop = kMaxRouterId;
                    mRouters[i].mCost = 0;
                    mRouters[i].mLastHeard = Timer::GetNow();
                }
                else if (mRouters[i].mNextHop == kMaxRouterId || mRouters[i].mNextHop == routerId)
                {
                    // route has no nexthop or nexthop is neighbor
                    newCost = cost + GetLinkCost(routerId);

                    if (i == routerId)
                    {
                        if (mRouters[i].mNextHop == kMaxRouterId)
                        {
                            ResetAdvertiseInterval();
                        }

                        mRouters[i].mNextHop = routerId;
                        mRouters[i].mCost = 0;
                    }
                    else if (newCost <= kMaxRouteCost)
                    {
                        if (mRouters[i].mNextHop == kMaxRouterId)
                        {
                            ResetAdvertiseInterval();
                        }

                        mRouters[i].mNextHop = routerId;
                        mRouters[i].mCost = cost;
                    }
                    else if (mRouters[i].mNextHop != kMaxRouterId)
                    {
                        ResetAdvertiseInterval();
                        mRouters[i].mNextHop = kMaxRouterId;
                        mRouters[i].mCost = 0;
                        mRouters[i].mLastHeard = Timer::GetNow();
                    }
                }
                else
                {
                    curCost = mRouters[i].mCost + GetLinkCost(mRouters[i].mNextHop);
                    newCost = cost + GetLinkCost(routerId);

                    if (newCost < curCost || (newCost == curCost && i == routerId))
                    {
                        mRouters[i].mNextHop = routerId;
                        mRouters[i].mCost = cost;
                    }
                }

                update |= mRouters[i].mNextHop != oldNextHop;
            }

            routeCount++;
        }
    }
    while (update);

#if 1

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mAllocated == false || mRouters[i].mNextHop == kMaxRouterId)
        {
            continue;
        }

        dprintf("%x: %x %d %d %d %d\n", GetRloc16(i), GetRloc16(mRouters[i].mNextHop),
                mRouters[i].mCost, GetLinkCost(i), mRouters[i].mLinkQualityIn, mRouters[i].mLinkQualityOut);
    }

#endif
}

ThreadError MleRouter::HandleParentRequest(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Mac::ExtAddress macAddr;
    VersionTlv version;
    ScanMaskTlv scanMask;
    ChallengeTlv challenge;
    Child *child;

    dprintf("Received parent request\n");

    // A Router MUST NOT send an MLE Parent Response if:

    // 1. It has no available Child capacity (if Max Child Count minus
    // Child Count would be equal to zero)
    // ==> verified below when allocating a child entry

    // 2. It is disconnected from its Partition (that is, it has not
    // received an updated ID sequence number within LEADER_TIMEOUT
    // seconds
    VerifyOrExit(GetLeaderAge() < mNetworkIdTimeout, error = kThreadError_Drop);

    // 3. Its current routing path cost to the Leader is infinite.
    VerifyOrExit(mRouters[GetLeaderId()].mNextHop != kMaxRouterId, error = kThreadError_Drop);

    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    // Version
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kVersion, sizeof(version), version));
    VerifyOrExit(version.IsValid() && version.GetVersion() == kVersion, error = kThreadError_Parse);

    // Scan Mask
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kScanMask, sizeof(scanMask), scanMask));
    VerifyOrExit(scanMask.IsValid(), error = kThreadError_Parse);

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        ExitNow();

    case kDeviceStateChild:
        VerifyOrExit(scanMask.IsChildFlagSet(), ;);
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        VerifyOrExit(scanMask.IsRouterFlagSet(), ;);
        break;
    }

    VerifyOrExit((child = FindChild(macAddr)) != NULL || (child = NewChild()) != NULL, ;);
    memset(child, 0, sizeof(*child));

    // Challenge
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge));
    VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);

    // MAC Address
    memcpy(&child->mMacAddr, &macAddr, sizeof(child->mMacAddr));

    child->mState = Neighbor::kStateParentRequest;
    child->mFramePending = false;
    child->mDataRequest = false;
    child->mPreviousKey = false;
    child->mRssi = reinterpret_cast<const ThreadMessageInfo *>(messageInfo.mLinkInfo)->mLinkMargin;
    child->mTimeout = 2 * kParentRequestChildTimeout * 1000U;
    SuccessOrExit(error = SendParentResponse(child, challenge));

exit:
    return error;
}

void MleRouter::HandleStateUpdateTimer(void *context)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleStateUpdateTimer();
}

void MleRouter::HandleStateUpdateTimer()
{
    uint8_t leaderId = GetLeaderId();

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
        assert(false);
        break;

    case kDeviceStateDetached:
        SetStateDetached();
        BecomeChild(kMleAttachAnyPartition);
        ExitNow();

    case kDeviceStateChild:
    case kDeviceStateRouter:
        // verify path to leader
        dprintf("network id timeout = %d\n", GetLeaderAge());

        if (GetLeaderAge() >= mNetworkIdTimeout)
        {
            BecomeChild(kMleAttachSamePartition);
        }

        break;

    case kDeviceStateLeader:

        // update router id sequence
        if (GetLeaderAge() >= kRouterIdSequencePeriod)
        {
            mRouterIdSequence++;
            mRouterIdSequenceLastUpdated = Timer::GetNow();
        }

        break;
    }

    // update children state
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState == Neighbor::kStateInvalid)
        {
            continue;
        }

        if ((Timer::GetNow() - mChildren[i].mLastHeard) >= mChildren[i].mTimeout * 1000U)
        {
            mChildren[i].mState = Neighbor::kStateInvalid;
        }
    }

    // update router state
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mState != Neighbor::kStateInvalid)
        {
            if ((Timer::GetNow() - mRouters[i].mLastHeard) >= kMaxNeighborAge * 1000U)
            {
                mRouters[i].mState = Neighbor::kStateInvalid;
                mRouters[i].mNextHop = kMaxRouterId;
                mRouters[i].mLinkQualityIn = 0;
                mRouters[i].mLinkQualityOut = 0;
                mRouters[i].mLastHeard = Timer::GetNow();
            }
        }

        if (GetDeviceState() == kDeviceStateLeader)
        {
            if (mRouters[i].mAllocated)
            {
                if (mRouters[i].mNextHop == kMaxRouterId &&
                    (Timer::GetNow() - mRouters[i].mLastHeard) >= kMaxLeaderToRouterTimeout * 1000U)
                {
                    ReleaseRouterId(i);
                }
            }
            else if (mRouters[i].mReclaimDelay)
            {
                if ((Timer::GetNow() - mRouters[i].mLastHeard) >= ((kMaxLeaderToRouterTimeout + kRouterIdReuseDelay) * 1000U))
                {
                    mRouters[i].mReclaimDelay = false;
                }
            }
        }
    }

    mStateUpdateTimer.Start(1000);

exit:
    {}
}

ThreadError MleRouter::SendParentResponse(Child *child, const ChallengeTlv &challenge)
{
    ThreadError error = kThreadError_None;
    Ip6::Address destination;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandParentResponse));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendLeaderData(*message));
    SuccessOrExit(error = AppendLinkFrameCounter(*message));
    SuccessOrExit(error = AppendMleFrameCounter(*message));
    SuccessOrExit(error = AppendResponse(*message, challenge.GetChallenge(), challenge.GetLength()));

    for (uint8_t i = 0; i < sizeof(child->mPending.mChallenge); i++)
    {
        child->mPending.mChallenge[i] = otRandomGet();
    }

    SuccessOrExit(error = AppendChallenge(*message, child->mPending.mChallenge, sizeof(child->mPending.mChallenge)));
    SuccessOrExit(error = AppendLinkMargin(*message, child->mRssi));
    SuccessOrExit(error = AppendConnectivity(*message));
    SuccessOrExit(error = AppendVersion(*message));

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xfe80);
    memcpy(destination.m8 + 8, &child->mMacAddr, sizeof(child->mMacAddr));
    destination.m8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Parent Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

ThreadError MleRouter::UpdateChildAddresses(const AddressRegistrationTlv &tlv, Child &child)
{
    const AddressRegistrationEntry *entry;
    Context context;

    memset(child.mIp6Address, 0, sizeof(child.mIp6Address));

    for (size_t count = 0; count < sizeof(child.mIp6Address) / sizeof(child.mIp6Address[0]); count++)
    {
        if ((entry = tlv.GetAddressEntry(count)) == NULL)
        {
            break;
        }

        if (entry->IsCompressed())
        {
            // xxx check if context id exists
            mNetworkData->GetContext(entry->GetContextId(), context);
            memcpy(&child.mIp6Address[count], context.mPrefix, (context.mPrefixLength + 7) / 8);
            memcpy(child.mIp6Address[count].m8 + 8, entry->GetIid(), 8);
        }
        else
        {
            memcpy(&child.mIp6Address[count], entry->GetIp6Address(), sizeof(child.mIp6Address[count]));
        }
    }

    return kThreadError_None;
}

ThreadError MleRouter::HandleChildIdRequest(const Message &message, const Ip6::MessageInfo &messageInfo,
                                            uint32_t keySequence)
{
    ThreadError error = kThreadError_None;
    Mac::ExtAddress macAddr;
    ResponseTlv response;
    LinkFrameCounterTlv linkFrameCounter;
    MleFrameCounterTlv mleFrameCounter;
    ModeTlv mode;
    TimeoutTlv timeout;
    AddressRegistrationTlv address;
    TlvRequestTlv tlvRequest;
    Child *child;

    dprintf("Received Child ID Request\n");

    // Find Child
    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    VerifyOrExit((child = FindChild(macAddr)) != NULL, ;);

    // Response
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kResponse, sizeof(response), response));
    VerifyOrExit(response.IsValid() &&
                 memcmp(response.GetResponse(), child->mPending.mChallenge, sizeof(child->mPending.mChallenge)) == 0, ;);

    // Link-Layer Frame Counter
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kLinkFrameCounter, sizeof(linkFrameCounter),
                                      linkFrameCounter));
    VerifyOrExit(linkFrameCounter.IsValid(), error = kThreadError_Parse);

    // MLE Frame Counter
    if (Tlv::GetTlv(message, Tlv::kMleFrameCounter, sizeof(mleFrameCounter), mleFrameCounter) ==
        kThreadError_None)
    {
        VerifyOrExit(mleFrameCounter.IsValid(), error = kThreadError_Parse);
    }
    else
    {
        mleFrameCounter.SetFrameCounter(linkFrameCounter.GetFrameCounter());
    }

    // Mode
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kMode, sizeof(mode), mode));
    VerifyOrExit(mode.IsValid(), error = kThreadError_Parse);

    // Timeout
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTimeout, sizeof(timeout), timeout));
    VerifyOrExit(timeout.IsValid(), error = kThreadError_Parse);

    // Ip6 Address
    address.SetLength(0);

    if ((mode.GetMode() & kModeFFD) == 0)
    {
        SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kAddressRegistration, sizeof(address), address));
        VerifyOrExit(address.IsValid(), error = kThreadError_Parse);
    }

    // TLV Request
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kTlvRequest, sizeof(tlvRequest), tlvRequest));
    VerifyOrExit(tlvRequest.IsValid(), error = kThreadError_Parse);

    // Remove from router table
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mState != Neighbor::kStateInvalid &&
            memcmp(&mRouters[i].mMacAddr, &macAddr, sizeof(mRouters[i].mMacAddr)) == 0)
        {
            mRouters[i].mState = Neighbor::kStateInvalid;
            break;
        }
    }

    child->mState = Neighbor::kStateChildIdRequest;
    child->mLastHeard = Timer::GetNow();
    child->mValid.mLinkFrameCounter = linkFrameCounter.GetFrameCounter();
    child->mValid.mMleFrameCounter = mleFrameCounter.GetFrameCounter();
    child->mMode = mode.GetMode();
    child->mTimeout = timeout.GetTimeout();

    if (mode.GetMode() & kModeFullNetworkData)
    {
        child->mNetworkDataVersion = mLeaderData.GetDataVersion();
    }
    else
    {
        child->mNetworkDataVersion = mLeaderData.GetStableDataVersion();
    }

    UpdateChildAddresses(address, *child);

    assert(keySequence == mKeyManager->GetCurrentKeySequence() ||
           keySequence == mKeyManager->GetPreviousKeySequence());
    child->mPreviousKey = keySequence == mKeyManager->GetPreviousKeySequence();

    for (uint8_t i = 0; i < tlvRequest.GetLength(); i++)
    {
        child->mRequestTlvs[i] = tlvRequest.GetTlvs()[i];
    }

    for (uint8_t i = tlvRequest.GetLength(); i < sizeof(child->mRequestTlvs); i++)
    {
        child->mRequestTlvs[i] = Tlv::kInvalid;
    }

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        BecomeRouter();
        break;

    case kDeviceStateRouter:
    case kDeviceStateLeader:
        SuccessOrExit(error = SendChildIdResponse(child));
        break;
    }

exit:
    return error;
}

ThreadError MleRouter::HandleChildUpdateRequest(const Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Mac::ExtAddress macAddr;
    ModeTlv mode;
    ChallengeTlv challenge;
    AddressRegistrationTlv address;
    LeaderDataTlv leaderData;
    TimeoutTlv timeout;
    Child *child;
    uint8_t tlvs[7];
    uint8_t tlvslength = 0;

    dprintf("Received Child Update Request\n");

    // Find Child
    memcpy(&macAddr, messageInfo.GetPeerAddr().m8 + 8, sizeof(macAddr));
    macAddr.mBytes[0] ^= 0x2;

    child = FindChild(macAddr);

    if (child == NULL)
    {
        tlvs[tlvslength++] = Tlv::kStatus;
        SendChildUpdateResponse(NULL, messageInfo, tlvs, tlvslength, NULL);
        ExitNow();
    }

    tlvs[tlvslength++] = Tlv::kSourceAddress;
    tlvs[tlvslength++] = Tlv::kLeaderData;

    // Mode
    SuccessOrExit(error = Tlv::GetTlv(message, Tlv::kMode, sizeof(mode), mode));
    VerifyOrExit(mode.IsValid(), error = kThreadError_Parse);
    child->mMode = mode.GetMode();
    tlvs[tlvslength++] = Tlv::kMode;

    // Challenge
    if (Tlv::GetTlv(message, Tlv::kChallenge, sizeof(challenge), challenge) == kThreadError_None)
    {
        VerifyOrExit(challenge.IsValid(), error = kThreadError_Parse);
        tlvs[tlvslength++] = Tlv::kResponse;
    }

    // Ip6 Address TLV
    if (Tlv::GetTlv(message, Tlv::kAddressRegistration, sizeof(address), address) == kThreadError_None)
    {
        VerifyOrExit(address.IsValid(), error = kThreadError_Parse);
        UpdateChildAddresses(address, *child);
        tlvs[tlvslength++] = Tlv::kAddressRegistration;
    }

    // Leader Data
    if (Tlv::GetTlv(message, Tlv::kLeaderData, sizeof(leaderData), leaderData) == kThreadError_None)
    {
        VerifyOrExit(leaderData.IsValid(), error = kThreadError_Parse);

        if (child->mMode & kModeFullNetworkData)
        {
            child->mNetworkDataVersion = leaderData.GetDataVersion();
        }
        else
        {
            child->mNetworkDataVersion = leaderData.GetStableDataVersion();
        }
    }

    // Timeout
    if (Tlv::GetTlv(message, Tlv::kTimeout, sizeof(timeout), timeout) == kThreadError_None)
    {
        VerifyOrExit(timeout.IsValid(), error = kThreadError_Parse);
        child->mTimeout = timeout.GetTimeout();
        tlvs[tlvslength++] = Tlv::kTimeout;
    }

    child->mLastHeard = Timer::GetNow();

    SendChildUpdateResponse(child, messageInfo, tlvs, tlvslength, &challenge);

exit:
    return error;
}

ThreadError MleRouter::HandleNetworkDataUpdateRouter()
{
    static const uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};
    Ip6::Address destination;

    VerifyOrExit(mDeviceState == kDeviceStateRouter || mDeviceState == kDeviceStateLeader, ;);

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xff02);
    destination.m16[7] = HostSwap16(0x0001);

    SendDataResponse(destination, tlvs, sizeof(tlvs));

exit:
    return kThreadError_None;
}

ThreadError MleRouter::SendChildIdResponse(Child *child)
{
    ThreadError error = kThreadError_None;
    Ip6::Address destination;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildIdResponse));
    SuccessOrExit(error = AppendSourceAddress(*message));
    SuccessOrExit(error = AppendLeaderData(*message));

    child->mValid.mRloc16 = mMesh->GetShortAddress() | mNextChildId;

    mNextChildId++;

    if (mNextChildId >= 512)
    {
        mNextChildId = 1;
    }

    SuccessOrExit(error = AppendAddress16(*message, child->mValid.mRloc16));

    for (uint8_t i = 0; i < sizeof(child->mRequestTlvs); i++)
    {
        switch (child->mRequestTlvs[i])
        {
        case Tlv::kNetworkData:
            SuccessOrExit(error = AppendNetworkData(*message, (child->mMode & kModeFullNetworkData) == 0));
            break;

        case Tlv::kRoute:
            SuccessOrExit(error = AppendRoute(*message));
            break;
        }
    }

    if ((child->mMode & kModeFFD) == 0)
    {
        SuccessOrExit(error = AppendChildAddresses(*message, *child));
    }

    child->mState = Neighbor::kStateValid;

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xfe80);
    memcpy(destination.m8 + 8, &child->mMacAddr, sizeof(child->mMacAddr));
    destination.m8[8] ^= 0x2;
    SuccessOrExit(error = SendMessage(*message, destination));

    dprintf("Sent Child ID Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

ThreadError MleRouter::SendChildUpdateResponse(Child *child, const Ip6::MessageInfo &messageInfo,
                                               const uint8_t *tlvs, uint8_t tlvslength,
                                               const ChallengeTlv *challenge)
{
    ThreadError error = kThreadError_None;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, ;);
    SuccessOrExit(error = AppendSecureHeader(*message, Header::kCommandChildUpdateResponse));

    for (int i = 0; i < tlvslength; i++)
    {
        switch (tlvs[i])
        {
        case Tlv::kStatus:
            SuccessOrExit(error = AppendStatus(*message, StatusTlv::kError));
            break;

        case Tlv::kAddressRegistration:
            SuccessOrExit(error = AppendChildAddresses(*message, *child));
            break;

        case Tlv::kLeaderData:
            SuccessOrExit(error = AppendLeaderData(*message));
            break;

        case Tlv::kMode:
            SuccessOrExit(error = AppendMode(*message, child->mMode));
            break;

        case Tlv::kResponse:
            SuccessOrExit(error = AppendResponse(*message, challenge->GetChallenge(), challenge->GetLength()));
            break;

        case Tlv::kSourceAddress:
            SuccessOrExit(error = AppendSourceAddress(*message));
            break;

        case Tlv::kTimeout:
            SuccessOrExit(error = AppendTimeout(*message, child->mTimeout));
            break;
        }
    }

    SuccessOrExit(error = SendMessage(*message, messageInfo.GetPeerAddr()));

    dprintf("Sent Child Update Response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }

    return kThreadError_None;
}

Child *MleRouter::GetChild(uint16_t address)
{
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState == Neighbor::kStateValid && mChildren[i].mValid.mRloc16 == address)
        {
            return &mChildren[i];
        }
    }

    return NULL;
}

Child *MleRouter::GetChild(const Mac::ExtAddress &address)
{
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState == Neighbor::kStateValid &&
            memcmp(&mChildren[i].mMacAddr, &address, sizeof(mChildren[i].mMacAddr)) == 0)
        {
            return &mChildren[i];
        }
    }

    return NULL;
}

Child *MleRouter::GetChild(const Mac::Address &address)
{
    switch (address.mLength)
    {
    case 2:
        return GetChild(address.mShortAddress);

    case 8:
        return GetChild(address.mExtAddress);
    }

    return NULL;
}

int MleRouter::GetChildIndex(const Child &child)
{
    return &child - mChildren;
}

Child *MleRouter::GetChildren(uint8_t *numChildren)
{
    if (numChildren != NULL)
    {
        *numChildren = kMaxChildren;
    }

    return mChildren;
}

Neighbor *MleRouter::GetNeighbor(uint16_t address)
{
    Neighbor *rval = NULL;

    if (address == Mac::kShortAddrBroadcast || address == Mac::kShortAddrInvalid)
    {
        ExitNow();
    }

    if (mDeviceState == kDeviceStateChild && (rval = Mle::GetNeighbor(address)) != NULL)
    {
        ExitNow();
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState == Neighbor::kStateValid && mChildren[i].mValid.mRloc16 == address)
        {
            ExitNow(rval = &mChildren[i]);
        }
    }

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mState == Neighbor::kStateValid && mRouters[i].mValid.mRloc16 == address)
        {
            ExitNow(rval = &mRouters[i]);
        }
    }

exit:
    return rval;
}

Neighbor *MleRouter::GetNeighbor(const Mac::ExtAddress &address)
{
    Neighbor *rval = NULL;

    if (mDeviceState == kDeviceStateChild && (rval = Mle::GetNeighbor(address)) != NULL)
    {
        ExitNow();
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState == Neighbor::kStateValid &&
            memcmp(&mChildren[i].mMacAddr, &address, sizeof(mChildren[i].mMacAddr)) == 0)
        {
            ExitNow(rval = &mChildren[i]);
        }
    }

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mState == Neighbor::kStateValid &&
            memcmp(&mRouters[i].mMacAddr, &address, sizeof(mRouters[i].mMacAddr)) == 0)
        {
            ExitNow(rval = &mRouters[i]);
        }
    }

exit:
    return rval;
}

Neighbor *MleRouter::GetNeighbor(const Mac::Address &address)
{
    Neighbor *rval = NULL;

    switch (address.mLength)
    {
    case 2:
        rval = GetNeighbor(address.mShortAddress);
        break;

    case 8:
        rval = GetNeighbor(address.mExtAddress);
        break;

    default:
        break;
    }

    return rval;
}

Neighbor *MleRouter::GetNeighbor(const Ip6::Address &address)
{
    Mac::Address macaddr;
    Context context;
    Child *child;
    Router *router;
    Neighbor *rval = NULL;

    if (address.IsLinkLocal())
    {
        if (address.m16[4] == HostSwap16(0x0000) &&
            address.m16[5] == HostSwap16(0x00ff) &&
            address.m16[6] == HostSwap16(0xfe00))
        {
            macaddr.mLength = 2;
            macaddr.mShortAddress = HostSwap16(address.m16[7]);
        }
        else
        {
            macaddr.mLength = 8;
            memcpy(macaddr.mExtAddress.mBytes, address.m8 + 8, sizeof(macaddr.mExtAddress));
            macaddr.mExtAddress.mBytes[0] ^= 0x02;
        }

        ExitNow(rval = GetNeighbor(macaddr));
    }

    if (mNetworkData->GetContext(address, context) != kThreadError_None)
    {
        context.mContextId = 0xff;
    }

    for (int i = 0; i < kMaxChildren; i++)
    {
        child = &mChildren[i];

        if (child->mState != Neighbor::kStateValid)
        {
            continue;
        }

        if (context.mContextId == 0 &&
            address.m16[4] == HostSwap16(0x0000) &&
            address.m16[5] == HostSwap16(0x00ff) &&
            address.m16[6] == HostSwap16(0xfe00) &&
            address.m16[7] == HostSwap16(child->mValid.mRloc16))
        {
            ExitNow(rval = child);
        }

        for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++)
        {
            if (memcmp(&child->mIp6Address[j], address.m8, sizeof(child->mIp6Address[j])) == 0)
            {
                ExitNow(rval = child);
            }
        }
    }

    VerifyOrExit(context.mContextId == 0, rval = NULL);

    for (int i = 0; i < kMaxRouterId; i++)
    {
        router = &mRouters[i];

        if (router->mState != Neighbor::kStateValid)
        {
            continue;
        }

        if (address.m16[4] == HostSwap16(0x0000) &&
            address.m16[5] == HostSwap16(0x00ff) &&
            address.m16[6] == HostSwap16(0xfe00) &&
            address.m16[7] == HostSwap16(router->mValid.mRloc16))
        {
            ExitNow(rval = router);
        }
    }

exit:
    return rval;
}

uint16_t MleRouter::GetNextHop(uint16_t destination) const
{
    uint8_t nexthop;

    if (mDeviceState == kDeviceStateChild)
    {
        return Mle::GetNextHop(destination);
    }

    nexthop = mRouters[GetRouterId(destination)].mNextHop;

    if (nexthop == kMaxRouterId || mRouters[nexthop].mState == Neighbor::kStateInvalid)
    {
        return Mac::kShortAddrInvalid;
    }

    return GetRloc16(nexthop);
}

uint8_t MleRouter::GetRouteCost(uint16_t rloc) const
{
    uint8_t routerId = GetRouterId(rloc);
    uint8_t rval;

    VerifyOrExit(routerId < kMaxRouterId && mRouters[routerId].mNextHop != kMaxRouterId, rval = kMaxRouteCost);

    rval = mRouters[routerId].mCost;

exit:
    return rval;
}

uint8_t MleRouter::GetRouterIdSequence() const
{
    return mRouterIdSequence;
}

uint8_t MleRouter::GetLeaderWeight() const
{
    return mLeaderWeight;
}

ThreadError MleRouter::SetLeaderWeight(uint8_t weight)
{
    mLeaderWeight = weight;
    return kThreadError_None;
}

ThreadError MleRouter::HandleMacDataRequest(const Child &child)
{
    static const uint8_t tlvs[] = {Tlv::kLeaderData, Tlv::kNetworkData};
    Ip6::Address destination;

    VerifyOrExit(child.mState == Neighbor::kStateValid && (child.mMode & kModeRxOnWhenIdle) == 0, ;);

    memset(&destination, 0, sizeof(destination));
    destination.m16[0] = HostSwap16(0xfe80);
    memcpy(destination.m8 + 8, &child.mMacAddr, sizeof(child.mMacAddr));
    destination.m8[8] ^= 0x2;

    if (child.mMode & kModeFullNetworkData)
    {
        if (child.mNetworkDataVersion != mNetworkData->GetVersion())
        {
            SendDataResponse(destination, tlvs, sizeof(tlvs));
        }
    }
    else
    {
        if (child.mNetworkDataVersion != mNetworkData->GetStableVersion())
        {
            SendDataResponse(destination, tlvs, sizeof(tlvs));
        }
    }

exit:
    return kThreadError_None;
}

Router *MleRouter::GetRouters(uint8_t *numRouters)
{
    if (numRouters != NULL)
    {
        *numRouters = kMaxRouterId;
    }

    return mRouters;
}

ThreadError MleRouter::CheckReachability(Mac::ShortAddress meshsrc, Mac::ShortAddress meshdst, Ip6::Header &ip6Header)
{
    Ip6::Address destination;

    if (mDeviceState == kDeviceStateChild)
    {
        return Mle::CheckReachability(meshsrc, meshdst, ip6Header);
    }

    if (meshdst == mMesh->GetShortAddress())
    {
        // mesh destination is this device
        if (mNetif->IsUnicastAddress(ip6Header.GetDestination()))
        {
            // IPv6 destination is this device
            return kThreadError_None;
        }
        else if (GetNeighbor(ip6Header.GetDestination()) != NULL)
        {
            // IPv6 destination is an RFD child
            return kThreadError_None;
        }
    }
    else if (GetRouterId(meshdst) == mRouterId)
    {
        // mesh destination is a child of this device
        if (GetChild(meshdst))
        {
            return kThreadError_None;
        }
    }
    else if (GetNextHop(meshdst) != Mac::kShortAddrInvalid)
    {
        // forwarding to another router and route is known
        return kThreadError_None;
    }

    memcpy(&destination, GetMeshLocal16(), 14);
    destination.m16[7] = HostSwap16(meshsrc);
    Ip6::Icmp::SendError(destination, Ip6::IcmpHeader::kTypeDstUnreach, Ip6::IcmpHeader::kCodeDstUnreachNoRoute,
                         ip6Header);

    return kThreadError_Drop;
}

ThreadError MleRouter::SendAddressSolicit()
{
    ThreadError error = kThreadError_None;
    Coap::Header header;
    ThreadMacAddr64Tlv macAddr64Tlv;
    ThreadRlocTlv rlocTlv;
    Ip6::MessageInfo messageInfo;
    Message *message;

    for (size_t i = 0; i < sizeof(mCoapToken); i++)
    {
        mCoapToken[i] = otRandomGet();
    }

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(mCoapToken, sizeof(mCoapToken));
    header.AppendUriPathOptions("a/as");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    macAddr64Tlv.Init();
    macAddr64Tlv.SetMacAddr(*mMesh->GetExtAddress());
    SuccessOrExit(error = message->Append(&macAddr64Tlv, sizeof(macAddr64Tlv)));

    if (mPreviousRouterId != kMaxRouterId)
    {
        rlocTlv.Init();
        rlocTlv.SetRloc16(GetRloc16(mPreviousRouterId));
        SuccessOrExit(error = message->Append(&rlocTlv, sizeof(rlocTlv)));
    }

    memset(&messageInfo, 0, sizeof(messageInfo));
    SuccessOrExit(error = GetLeaderAddress(messageInfo.GetPeerAddr()));
    messageInfo.mPeerPort = kCoapUdpPort;
    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    dprintf("Sent address solicit to %04x\n", HostSwap16(messageInfo.GetPeerAddr().m16[7]));

exit:
    return error;
}

ThreadError MleRouter::SendAddressRelease()
{
    ThreadError error = kThreadError_None;
    Coap::Header header;
    ThreadRlocTlv rlocTlv;
    ThreadMacAddr64Tlv macAddr64Tlv;
    Ip6::MessageInfo messageInfo;
    Message *message;

    for (size_t i = 0; i < sizeof(mCoapToken); i++)
    {
        mCoapToken[i] = otRandomGet();
    }

    header.SetVersion(1);
    header.SetType(Coap::Header::kTypeConfirmable);
    header.SetCode(Coap::Header::kCodePost);
    header.SetMessageId(++mCoapMessageId);
    header.SetToken(mCoapToken, sizeof(mCoapToken));
    header.AppendUriPathOptions("a/ar");
    header.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    header.Finalize();

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    SuccessOrExit(error = message->Append(header.GetBytes(), header.GetLength()));

    rlocTlv.Init();
    rlocTlv.SetRloc16(GetRloc16(mRouterId));
    SuccessOrExit(error = message->Append(&rlocTlv, sizeof(rlocTlv)));

    macAddr64Tlv.Init();
    macAddr64Tlv.SetMacAddr(*mMesh->GetExtAddress());
    SuccessOrExit(error = message->Append(&macAddr64Tlv, sizeof(macAddr64Tlv)));

    memset(&messageInfo, 0, sizeof(messageInfo));
    SuccessOrExit(error = GetLeaderAddress(messageInfo.GetPeerAddr()));
    messageInfo.mPeerPort = kCoapUdpPort;
    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    dprintf("Sent address release\n");

exit:
    return error;
}

void MleRouter::HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleUdpReceive(*static_cast<Message *>(message), *static_cast<const Ip6::MessageInfo *>(messageInfo));
}

void MleRouter::HandleUdpReceive(Message &message, const Ip6::MessageInfo &messageInfo)
{
    HandleAddressSolicitResponse(message);
}

void MleRouter::HandleAddressSolicitResponse(Message &message)
{
    Coap::Header header;
    ThreadStatusTlv statusTlv;
    ThreadRlocTlv rlocTlv;
    ThreadRouterMaskTlv routerMaskTlv;
    bool old;

    SuccessOrExit(header.FromMessage(message));
    VerifyOrExit(header.GetType() == Coap::Header::kTypeAcknowledgment &&
                 header.GetCode() == Coap::Header::kCodeChanged &&
                 header.GetMessageId() == mCoapMessageId &&
                 header.GetTokenLength() == sizeof(mCoapToken) &&
                 memcmp(mCoapToken, header.GetToken(), sizeof(mCoapToken)) == 0, ;);
    message.MoveOffset(header.GetLength());

    dprintf("Received address reply\n");

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kStatus, sizeof(statusTlv), statusTlv));
    VerifyOrExit(statusTlv.IsValid() && statusTlv.GetStatus() == statusTlv.kSuccess, ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rlocTlv), rlocTlv));
    VerifyOrExit(rlocTlv.IsValid(), ;);

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRouterMask, sizeof(routerMaskTlv), routerMaskTlv));
    VerifyOrExit(routerMaskTlv.IsValid(), ;);

    // assign short address
    mRouterId = GetRouterId(rlocTlv.GetRloc16());
    mPreviousRouterId = mRouterId;
    SuccessOrExit(SetStateRouter(GetRloc16(mRouterId)));
    mRouters[mRouterId].mCost = 0;

    // copy router id information
    mRouterIdSequence = routerMaskTlv.GetRouterIdSequence();
    mRouterIdSequenceLastUpdated = Timer::GetNow();

    for (int i = 0; i < kMaxRouterId; i++)
    {
        old = mRouters[i].mAllocated;
        mRouters[i].mAllocated = routerMaskTlv.IsRouterIdSet(i);

        if (old && !mRouters[i].mAllocated)
        {
            mAddressResolver->Remove(i);
        }
    }

    // send link request
    SendLinkRequest(NULL);
    ResetAdvertiseInterval();

    // send child id responses
    for (int i = 0; i < kMaxChildren; i++)
    {
        if (mChildren[i].mState == Neighbor::kStateChildIdRequest)
        {
            SendChildIdResponse(&mChildren[i]);
        }
    }

exit:
    {}
}

void MleRouter::HandleAddressSolicit(void *context, Coap::Header &header, Message &message,
                                     const Ip6::MessageInfo &messageInfo)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleAddressSolicit(header, message, messageInfo);
}

void MleRouter::HandleAddressSolicit(Coap::Header &header, Message &message, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    ThreadMacAddr64Tlv macAddr64Tlv;
    ThreadRlocTlv rlocTlv;
    int routerId;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address solicit\n");

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kMacAddr64, sizeof(macAddr64Tlv), macAddr64Tlv));
    VerifyOrExit(macAddr64Tlv.IsValid(), error = kThreadError_Parse);

    routerId = -1;

    // see if allocation already exists
    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mAllocated &&
            memcmp(&mRouters[i].mMacAddr, macAddr64Tlv.GetMacAddr(), sizeof(mRouters[i].mMacAddr)) == 0)
        {
            SendAddressSolicitResponse(header, i, messageInfo);
            ExitNow();
        }
    }

    if (ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rlocTlv), rlocTlv) == kThreadError_None)
    {
        // specific Router ID requested
        VerifyOrExit(rlocTlv.IsValid(), error = kThreadError_Parse);
        routerId = GetRouterId(rlocTlv.GetRloc16());

        if (routerId >= kMaxRouterId)
        {
            // requested Router ID is out of range
            routerId = -1;
        }
        else if (mRouters[routerId].mAllocated &&
                 memcmp(&mRouters[routerId].mMacAddr, macAddr64Tlv.GetMacAddr(),
                        sizeof(mRouters[routerId].mMacAddr)))
        {
            // requested Router ID is allocated to another device
            routerId = -1;
        }
        else if (!mRouters[routerId].mAllocated && mRouters[routerId].mReclaimDelay)
        {
            // requested Router ID is deallocated but within ID_REUSE_DELAY period
            routerId = -1;
        }
        else
        {
            routerId = AllocateRouterId(routerId);
        }
    }

    // allocate new router id
    if (routerId < 0)
    {
        routerId = AllocateRouterId();
    }
    else
    {
        dprintf("router id requested and provided!\n");
    }

    if (routerId >= 0)
    {
        memcpy(&mRouters[routerId].mMacAddr, macAddr64Tlv.GetMacAddr(), sizeof(mRouters[routerId].mMacAddr));
    }
    else
    {
        dprintf("router address unavailable!\n");
    }

    SendAddressSolicitResponse(header, routerId, messageInfo);

exit:
    {}
}

void MleRouter::SendAddressSolicitResponse(const Coap::Header &requestHeader, int routerId,
                                           const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Coap::Header responseHeader;
    ThreadStatusTlv statusTlv;
    ThreadRouterMaskTlv routerMaskTlv;
    ThreadRlocTlv rlocTlv;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    responseHeader.Init();
    responseHeader.SetVersion(1);
    responseHeader.SetType(Coap::Header::kTypeAcknowledgment);
    responseHeader.SetCode(Coap::Header::kCodeChanged);
    responseHeader.SetMessageId(requestHeader.GetMessageId());
    responseHeader.SetToken(requestHeader.GetToken(), requestHeader.GetTokenLength());
    responseHeader.AppendContentFormatOption(Coap::Header::kApplicationOctetStream);
    responseHeader.Finalize();
    SuccessOrExit(error = message->Append(responseHeader.GetBytes(), responseHeader.GetLength()));

    statusTlv.Init();
    statusTlv.SetStatus((routerId < 0) ? statusTlv.kNoAddressAvailable : statusTlv.kSuccess);
    SuccessOrExit(error = message->Append(&statusTlv, sizeof(statusTlv)));

    if (routerId >= 0)
    {
        rlocTlv.Init();
        rlocTlv.SetRloc16(GetRloc16(routerId));
        SuccessOrExit(error = message->Append(&rlocTlv, sizeof(rlocTlv)));

        routerMaskTlv.Init();
        routerMaskTlv.SetRouterIdSequence(mRouterIdSequence);
        routerMaskTlv.ClearRouterIdMask();

        for (int i = 0; i < kMaxRouterId; i++)
        {
            if (mRouters[i].mAllocated)
            {
                routerMaskTlv.SetRouterId(i);
            }
        }

        SuccessOrExit(error = message->Append(&routerMaskTlv, sizeof(routerMaskTlv)));
    }

    SuccessOrExit(error = mCoapServer->SendMessage(*message, messageInfo));

    dprintf("Sent address reply\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

void MleRouter::HandleAddressRelease(void *context, Coap::Header &header, Message &message,
                                     const Ip6::MessageInfo &messageInfo)
{
    MleRouter *obj = reinterpret_cast<MleRouter *>(context);
    obj->HandleAddressRelease(header, message, messageInfo);
}

void MleRouter::HandleAddressRelease(Coap::Header &header, Message &message,
                                     const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    ThreadRlocTlv rlocTlv;
    ThreadMacAddr64Tlv macAddr64Tlv;
    uint8_t routerId;
    Router *router;

    VerifyOrExit(header.GetType() == Coap::Header::kTypeConfirmable &&
                 header.GetCode() == Coap::Header::kCodePost, ;);

    dprintf("Received address release\n");

    SuccessOrExit(ThreadTlv::GetTlv(message, ThreadTlv::kRloc, sizeof(rlocTlv), rlocTlv));
    VerifyOrExit(rlocTlv.IsValid(), ;);

    SuccessOrExit(error = ThreadTlv::GetTlv(message, ThreadTlv::kMacAddr64, sizeof(macAddr64Tlv), macAddr64Tlv));
    VerifyOrExit(macAddr64Tlv.IsValid(), error = kThreadError_Parse);

    routerId = GetRouterId(rlocTlv.GetRloc16());
    router = &mRouters[routerId];
    VerifyOrExit(memcmp(&router->mMacAddr, macAddr64Tlv.GetMacAddr(), sizeof(router->mMacAddr)) == 0, ;);

    ReleaseRouterId(routerId);
    SendAddressReleaseResponse(header, messageInfo);

exit:
    {}
}

void MleRouter::SendAddressReleaseResponse(const Coap::Header &requestHeader, const Ip6::MessageInfo &messageInfo)
{
    ThreadError error = kThreadError_None;
    Coap::Header responseHeader;
    Message *message;

    VerifyOrExit((message = Ip6::Udp::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
    responseHeader.Init();
    responseHeader.SetVersion(1);
    responseHeader.SetType(Coap::Header::kTypeAcknowledgment);
    responseHeader.SetCode(Coap::Header::kCodeChanged);
    responseHeader.SetMessageId(requestHeader.GetMessageId());
    responseHeader.SetToken(requestHeader.GetToken(), requestHeader.GetTokenLength());
    responseHeader.Finalize();
    SuccessOrExit(error = message->Append(responseHeader.GetBytes(), responseHeader.GetLength()));

    SuccessOrExit(error = mCoapServer->SendMessage(*message, messageInfo));

    dprintf("Sent address release response\n");

exit:

    if (error != kThreadError_None && message != NULL)
    {
        Message::Free(*message);
    }
}

ThreadError MleRouter::AppendConnectivity(Message &message)
{
    ThreadError error;
    ConnectivityTlv tlv;
    uint8_t cost;
    uint8_t lqi;

    tlv.Init();
    tlv.SetMaxChildCount(kMaxChildren);

    // compute number of children
    tlv.SetChildCount(0);

    for (int i = 0; i < kMaxChildren; i++)
    {
        tlv.SetChildCount(tlv.GetChildCount() + mChildren[i].mState == Neighbor::kStateValid);
    }

    // compute leader cost and link qualities
    tlv.SetLinkQuality1(0);
    tlv.SetLinkQuality2(0);
    tlv.SetLinkQuality3(0);

    cost = mRouters[GetLeaderId()].mCost;

    switch (GetDeviceState())
    {
    case kDeviceStateDisabled:
    case kDeviceStateDetached:
        assert(false);
        break;

    case kDeviceStateChild:
        switch (mParent.mLinkQualityIn)
        {
        case 1:
            tlv.SetLinkQuality1(tlv.GetLinkQuality1() + 1);
            break;

        case 2:
            tlv.SetLinkQuality2(tlv.GetLinkQuality2() + 1);
            break;

        case 3:
            tlv.SetLinkQuality3(tlv.GetLinkQuality3() + 1);
            break;
        }

        cost += LQI_TO_COST(mParent.mLinkQualityIn);
        break;

    case kDeviceStateRouter:
        cost += GetLinkCost(mRouters[GetLeaderId()].mNextHop);
        break;

    case kDeviceStateLeader:
        cost = 0;
        break;
    }

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mState != Neighbor::kStateValid || i == mRouterId)
        {
            continue;
        }

        lqi = mRouters[i].mLinkQualityIn;

        if (lqi > mRouters[i].mLinkQualityOut)
        {
            lqi = mRouters[i].mLinkQualityOut;
        }

        switch (lqi)
        {
        case 1:
            tlv.SetLinkQuality1(tlv.GetLinkQuality1() + 1);
            break;

        case 2:
            tlv.SetLinkQuality2(tlv.GetLinkQuality2() + 1);
            break;

        case 3:
            tlv.SetLinkQuality3(tlv.GetLinkQuality3() + 1);
            break;
        }
    }

    tlv.SetLeaderCost((cost < kMaxRouteCost) ? cost : kMaxRouteCost);
    tlv.SetRouterIdSequence(mRouterIdSequence);

    SuccessOrExit(error = message.Append(&tlv, sizeof(tlv)));

exit:
    return error;
}

ThreadError MleRouter::AppendChildAddresses(Message &message, Child &child)
{
    ThreadError error;
    Tlv tlv;
    AddressRegistrationEntry entry;
    Context context;
    uint8_t length = 0;

    tlv.SetType(Tlv::kAddressRegistration);

    // compute size of TLV
    for (size_t i = 0; i < sizeof(child.mIp6Address) / sizeof(child.mIp6Address[0]); i++)
    {
        if (mNetworkData->GetContext(child.mIp6Address[i], context) == kThreadError_None)
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

    for (size_t i = 0; i < sizeof(child.mIp6Address) / sizeof(child.mIp6Address[0]); i++)
    {
        if (mNetworkData->GetContext(child.mIp6Address[i], context) == kThreadError_None)
        {
            // compressed entry
            entry.SetContextId(context.mContextId);
            entry.SetIid(child.mIp6Address[i].m8 + 8);
            length = 9;
        }
        else
        {
            // uncompressed entry
            entry.SetUncompressed();
            entry.SetIp6Address(child.mIp6Address[i]);
            length = 17;
        }

        SuccessOrExit(error = message.Append(&entry, length));
    }

exit:
    return error;
}

ThreadError MleRouter::AppendRoute(Message &message)
{
    ThreadError error;
    RouteTlv tlv;
    int routeCount = 0;
    uint8_t cost;

    tlv.Init();
    tlv.SetRouterIdSequence(mRouterIdSequence);
    tlv.ClearRouterIdMask();

    for (int i = 0; i < kMaxRouterId; i++)
    {
        if (mRouters[i].mAllocated == false)
        {
            continue;
        }

        tlv.SetRouterId(i);

        if (i == mRouterId)
        {
            tlv.SetLinkQualityIn(routeCount, 0);
            tlv.SetLinkQualityOut(routeCount, 0);
            tlv.SetRouteCost(routeCount, 1);
        }
        else
        {
            if (mRouters[i].mNextHop == kMaxRouterId)
            {
                cost = 0;
            }
            else
            {
                cost = mRouters[i].mCost + GetLinkCost(mRouters[i].mNextHop);

                if (cost >= kMaxRouteCost)
                {
                    cost = 0;
                }
            }

            tlv.SetRouteCost(routeCount, cost);
            tlv.SetLinkQualityIn(routeCount, mRouters[i].mLinkQualityIn);
            tlv.SetLinkQualityOut(routeCount, mRouters[i].mLinkQualityOut);
        }

        routeCount++;
    }

    tlv.SetRouteDataLength(routeCount);
    SuccessOrExit(error = message.Append(&tlv, sizeof(Tlv) + tlv.GetLength()));

exit:
    return error;
}

}  // namespace Mle
}  // namespace Thread
