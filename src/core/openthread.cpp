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

#include <openthread.h>
#include <common/code_utils.hpp>
#include <common/random.hpp>
#include <common/message.hpp>
#include <common/tasklet.hpp>
#include <common/timer.hpp>
#include <thread/thread_netif.hpp>

namespace Thread {

ThreadNetif sThreadNetif;

#ifdef __cplusplus
extern "C" {
#endif

void otInit(uint32_t seed)
{
    Message::Init();
    Random::Init(seed);
    Timer::Init();

    sThreadNetif.Init();
}

void otProcessNextTasklet(void)
{
    TaskletScheduler::RunNextTasklet();
}

bool otAreTaskletsPending(void)
{
    return TaskletScheduler::AreTaskletsPending();
}

uint8_t otGetChannel(void)
{
    return sThreadNetif.GetMac()->GetChannel();
}

ThreadError otSetChannel(uint8_t aChannel)
{
    return sThreadNetif.GetMac()->SetChannel(aChannel);
}

uint32_t otGetChildTimeout(void)
{
    return sThreadNetif.GetMle()->GetTimeout();
}

void otSetChildTimeout(uint32_t aTimeout)
{
    sThreadNetif.GetMle()->SetTimeout(aTimeout);
}

const uint8_t *otGetExtendedAddress(void)
{
    return reinterpret_cast<const uint8_t *>(sThreadNetif.GetMac()->GetAddress64());
}

const uint8_t *otGetExtendedPanId(void)
{
    return sThreadNetif.GetMac()->GetExtendedPanId();
}

void otSetExtendedPanId(const uint8_t *aExtendedPanId)
{
    sThreadNetif.GetMac()->SetExtendedPanId(aExtendedPanId);
}

otLinkModeConfig otGetLinkMode(void)
{
    otLinkModeConfig config = {};
    uint8_t mode = sThreadNetif.GetMle()->GetDeviceMode();

    if (mode & Mle::ModeTlv::kModeRxOnWhenIdle)
    {
        config.mRxOnWhenIdle = 1;
    }

    if (mode & Mle::ModeTlv::kModeSecureDataRequest)
    {
        config.mSecureDataRequests = 1;
    }

    if (mode & Mle::ModeTlv::kModeFFD)
    {
        config.mDeviceType = 1;
    }

    if (mode & Mle::ModeTlv::kModeFullNetworkData)
    {
        config.mNetworkData = 1;
    }

    return config;
}

ThreadError otSetLinkMode(otLinkModeConfig aConfig)
{
    uint8_t mode = 0;

    if (aConfig.mRxOnWhenIdle)
    {
        mode |= Mle::ModeTlv::kModeRxOnWhenIdle;
    }

    if (aConfig.mSecureDataRequests)
    {
        mode |= Mle::ModeTlv::kModeSecureDataRequest;
    }

    if (aConfig.mDeviceType)
    {
        mode |= Mle::ModeTlv::kModeFFD;
    }

    if (aConfig.mNetworkData)
    {
        mode |= Mle::ModeTlv::kModeFullNetworkData;
    }

    return sThreadNetif.GetMle()->SetDeviceMode(mode);
}

const uint8_t *otGetMasterKey(uint8_t *aKeyLength)
{
    return sThreadNetif.GetKeyManager()->GetMasterKey(aKeyLength);
}

ThreadError otSetMasterKey(const uint8_t *aKey, uint8_t aKeyLength)
{
    return sThreadNetif.GetKeyManager()->SetMasterKey(aKey, aKeyLength);
}

const char *otGetNetworkName(void)
{
    return sThreadNetif.GetMac()->GetNetworkName();
}

ThreadError otSetNetworkName(const char *aNetworkName)
{
    return sThreadNetif.GetMac()->SetNetworkName(aNetworkName);
}

uint16_t otGetPanId(void)
{
    return sThreadNetif.GetMac()->GetPanId();
}

ThreadError otSetPanId(uint16_t aPanId)
{
    return sThreadNetif.GetMac()->SetPanId(aPanId);
}

uint8_t otGetLocalLeaderWeight(void)
{
    return sThreadNetif.GetMle()->GetLeaderWeight();
}

void otSetLocalLeaderWeight(uint8_t aWeight)
{
    sThreadNetif.GetMle()->SetLeaderWeight(aWeight);
}

ThreadError otAddBorderRouter(const otBorderRouterConfig *aConfig)
{
    uint8_t flags = 0;

    if (aConfig->mSlaacPreferred)
    {
        flags |= NetworkData::BorderRouterEntry::kPreferredFlag;
    }

    if (aConfig->mSlaacValid)
    {
        flags |= NetworkData::BorderRouterEntry::kValidFlag;
    }

    if (aConfig->mDhcp)
    {
        flags |= NetworkData::BorderRouterEntry::kDhcpFlag;
    }

    if (aConfig->mConfigure)
    {
        flags |= NetworkData::BorderRouterEntry::kConfigureFlag;
    }

    if (aConfig->mDefaultRoute)
    {
        flags |= NetworkData::BorderRouterEntry::kDefaultRouteFlag;
    }

    return sThreadNetif.GetNetworkDataLocal()->AddOnMeshPrefix(aConfig->mPrefix.mPrefix.m8, aConfig->mPrefix.mLength,
                                                               aConfig->mPreference, flags, aConfig->mStable);
}

ThreadError otRemoveBorderRouter(const otIp6Prefix *aPrefix)
{
    return sThreadNetif.GetNetworkDataLocal()->RemoveOnMeshPrefix(aPrefix->mPrefix.m8, aPrefix->mLength);
}

ThreadError otAddExternalRoute(const otExternalRouteConfig *aConfig)
{
    return sThreadNetif.GetNetworkDataLocal()->AddHasRoutePrefix(aConfig->mPrefix.mPrefix.m8, aConfig->mPrefix.mLength,
                                                                 aConfig->mPreference, aConfig->mStable);
}

ThreadError otRemoveExternalRoute(const otIp6Prefix *aPrefix)
{
    return sThreadNetif.GetNetworkDataLocal()->RemoveHasRoutePrefix(aPrefix->mPrefix.m8, aPrefix->mLength);
}

ThreadError otSendServerData(void)
{
    Ip6Address destination;
    sThreadNetif.GetMle()->GetLeaderAddress(destination);
    return sThreadNetif.GetNetworkDataLocal()->Register(destination);
}

uint32_t otGetContextIdReuseDelay(void)
{
    return sThreadNetif.GetNetworkDataLeader()->GetContextIdReuseDelay();
}

void otSetContextIdReuseDelay(uint32_t aDelay)
{
    sThreadNetif.GetNetworkDataLeader()->SetContextIdReuseDelay(aDelay);
}

uint32_t otGetKeySequenceCounter(void)
{
    return sThreadNetif.GetKeyManager()->GetCurrentKeySequence();
}

void otSetKeySequenceCounter(uint32_t aKeySequenceCounter)
{
    sThreadNetif.GetKeyManager()->SetCurrentKeySequence(aKeySequenceCounter);
}

uint32_t otGetNetworkIdTimeout(void)
{
    return sThreadNetif.GetMle()->GetNetworkIdTimeout();
}

void otSetNetworkIdTimeout(uint32_t aTimeout)
{
    sThreadNetif.GetMle()->SetNetworkIdTimeout(aTimeout);
}

uint8_t otGetRouterUpgradeThreshold(void)
{
    return sThreadNetif.GetMle()->GetRouterUpgradeThreshold();
}

void otSetRouterUpgradeThreshold(uint8_t aThreshold)
{
    sThreadNetif.GetMle()->SetRouterUpgradeThreshold(aThreshold);
}

ThreadError otReleaseRouterId(uint8_t aRouterId)
{
    return sThreadNetif.GetMle()->ReleaseRouterId(aRouterId);
}

ThreadError otAddMacWhitelist(const uint8_t *aExtAddr)
{
    ThreadError error = kThreadError_None;
    int entry = sThreadNetif.GetMac()->GetWhitelist()->Add(*reinterpret_cast<const Mac::Address64 *>(aExtAddr));

    if (entry < 0)
    {
        error = kThreadError_NoBufs;
    }

    return error;
}

ThreadError otAddMacWhitelistRssi(const uint8_t *aExtAddr, int8_t aRssi)
{
    ThreadError error = kThreadError_None;
    int entry = sThreadNetif.GetMac()->GetWhitelist()->Add(*reinterpret_cast<const Mac::Address64 *>(aExtAddr));
    VerifyOrExit(entry >= 0, error = kThreadError_NoBufs);
    error = sThreadNetif.GetMac()->GetWhitelist()->SetRssi(entry, aRssi);
exit:
    return error;
}

ThreadError otRemoveMacWhitelist(const uint8_t *aExtAddr)
{
    return sThreadNetif.GetMac()->GetWhitelist()->Remove(*reinterpret_cast<const Mac::Address64 *>(aExtAddr));
}

void otClearMacWhitelist()
{
    sThreadNetif.GetMac()->GetWhitelist()->Clear();
}

void otDisableMacWhitelist()
{
    sThreadNetif.GetMac()->GetWhitelist()->Disable();
}

void otEnableMacWhitelist()
{
    sThreadNetif.GetMac()->GetWhitelist()->Enable();
}

ThreadError otBecomeDetached()
{
    return sThreadNetif.GetMle()->BecomeDetached();
}

ThreadError otBecomeChild(AttachFilter aFilter)
{
    return sThreadNetif.GetMle()->BecomeChild(aFilter);
}

ThreadError otBecomeRouter()
{
    return sThreadNetif.GetMle()->BecomeRouter();
}

ThreadError otBecomeLeader()
{
    return sThreadNetif.GetMle()->BecomeLeader();
}

otDeviceRole otGetDeviceRole()
{
    otDeviceRole rval;

    switch (sThreadNetif.GetMle()->GetDeviceState())
    {
    case Mle::kDeviceStateDisabled:
        rval = kDeviceRoleDisabled;
        break;

    case Mle::kDeviceStateDetached:
        rval = kDeviceRoleDetached;
        break;

    case Mle::kDeviceStateChild:
        rval = kDeviceRoleChild;
        break;

    case Mle::kDeviceStateRouter:
        rval = kDeviceRoleRouter;
        break;

    case Mle::kDeviceStateLeader:
        rval = kDeviceRoleLeader;
        break;
    }

    return rval;
}

uint8_t otGetLeaderRouterId()
{
    return sThreadNetif.GetMle()->GetLeaderDataTlv()->GetRouterId();
}

uint8_t otGetLeaderWeight()
{
    return sThreadNetif.GetMle()->GetLeaderDataTlv()->GetWeighting();
}

uint8_t otGetNetworkDataVersion()
{
    return sThreadNetif.GetMle()->GetLeaderDataTlv()->GetDataVersion();
}

uint32_t otGetPartitionId()
{
    return sThreadNetif.GetMle()->GetLeaderDataTlv()->GetPartitionId();
}

uint16_t otGetRloc16(void)
{
    return sThreadNetif.GetMle()->GetRloc16();
}

uint8_t otGetRouterIdSequence()
{
    return sThreadNetif.GetMle()->GetRouterIdSequence();
}

uint8_t otGetStableNetworkDataVersion()
{
    return sThreadNetif.GetMle()->GetLeaderDataTlv()->GetStableDataVersion();
}

bool otIsIp6AddressEqual(const otIp6Address *a, const otIp6Address *b)
{
    return *static_cast<const Ip6Address *>(a) == *static_cast<const Ip6Address *>(b);
}

ThreadError otIp6AddressFromString(const char *str, otIp6Address *address)
{
    return static_cast<Ip6Address *>(address)->FromString(str);
}

const otNetifAddress *otGetUnicastAddresses()
{
    return sThreadNetif.GetUnicastAddresses();
}

ThreadError otAddUnicastAddress(otNetifAddress *address)
{
    return sThreadNetif.AddUnicastAddress(*static_cast<NetifUnicastAddress *>(address));
}

ThreadError otRemoveUnicastAddress(otNetifAddress *address)
{
    return sThreadNetif.RemoveUnicastAddress(*static_cast<NetifUnicastAddress *>(address));
}

ThreadError otEnable(void)
{
    return sThreadNetif.Up();
}

ThreadError otDisable(void)
{
    return sThreadNetif.Down();
}

otMessage otNewUdp6Message()
{
    return Udp6::NewMessage(0);
}

ThreadError otFreeMessage(otMessage aMessage)
{
    return Message::Free(*static_cast<Message *>(aMessage));
}

uint16_t otGetMessageLength(otMessage aMessage)
{
    Message *message = static_cast<Message *>(aMessage);
    return message->GetLength();
}

ThreadError otSetMessageLength(otMessage aMessage, uint16_t aLength)
{
    Message *message = static_cast<Message *>(aMessage);
    return message->SetLength(aLength);
}

uint16_t otGetMessageOffset(otMessage aMessage)
{
    Message *message = static_cast<Message *>(aMessage);
    return message->GetOffset();
}

ThreadError otSetMessageOffset(otMessage aMessage, uint16_t aOffset)
{
    Message *message = static_cast<Message *>(aMessage);
    return message->SetOffset(aOffset);
}

int otAppendMessage(otMessage aMessage, const void *aBuf, uint16_t aLength)
{
    Message *message = static_cast<Message *>(aMessage);
    return message->Append(aBuf, aLength);
}

int otReadMessage(otMessage aMessage, uint16_t aOffset, void *aBuf, uint16_t aLength)
{
    Message *message = static_cast<Message *>(aMessage);
    return message->Read(aOffset, aLength, aBuf);
}

int otWriteMessage(otMessage aMessage, uint16_t aOffset, const void *aBuf, uint16_t aLength)
{
    Message *message = static_cast<Message *>(aMessage);
    return message->Write(aOffset, aLength, aBuf);
}

ThreadError otOpenUdp6Socket(otUdp6Socket *aSocket, otUdp6Receive aCallback, void *aContext)
{
    Udp6Socket *socket = reinterpret_cast<Udp6Socket *>(aSocket);
    return socket->Open(aCallback, aContext);
}

ThreadError otCloseUdp6Socket(otUdp6Socket *aSocket)
{
    Udp6Socket *socket = reinterpret_cast<Udp6Socket *>(aSocket);
    return socket->Close();
}

ThreadError otBindUdp6Socket(otUdp6Socket *aSocket, otSockAddr *aSockName)
{
    Udp6Socket *socket = reinterpret_cast<Udp6Socket *>(aSocket);
    return socket->Bind(*reinterpret_cast<const SockAddr *>(aSockName));
}

ThreadError otSendUdp6Message(otUdp6Socket *aSocket, otMessage aMessage, const otMessageInfo *aMessageInfo)
{
    Udp6Socket *socket = reinterpret_cast<Udp6Socket *>(aSocket);
    return socket->SendTo(*reinterpret_cast<Message *>(aMessage),
                          *reinterpret_cast<const Ip6MessageInfo *>(aMessageInfo));
}

#ifdef __cplusplus
}  // extern "C"
#endif

}  // namespace Thread
