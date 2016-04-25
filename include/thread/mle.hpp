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
 *   This file includes definitions for MLE functionality required by the Thread Child, Router, and Leader roles.
 */

#ifndef MLE_HPP_
#define MLE_HPP_

#include <openthread.h>
#include <common/encoding.hpp>
#include <common/timer.hpp>
#include <crypto/aes_ecb.hpp>
#include <mac/mac.hpp>
#include <net/udp6.hpp>
#include <thread/mle_tlvs.hpp>
#include <thread/topology.hpp>

namespace Thread {

class ThreadNetif;
class AddressResolver;
class KeyManager;
class MeshForwarder;
namespace NetworkData { class Leader; }

/**
 * @addtogroup core-mle MLE
 *
 * @brief
 *   This module includes definitions for the MLE protocol.
 *
 * @{
 *
 * @defgroup core-mle-core Core
 * @defgroup core-mle-router Router
 * @defgroup core-mle-tlvs TLVs
 *
 * @}
 */

/**
 * @namespace Thread::Mle
 *
 * @brief
 *   This namespace includes definitions for the MLE protocol.
 */

namespace Mle {

class MleRouter;

/**
 * @addtogroup core-mle-core
 *
 * @brief
 *   This module includes definitions for MLE functionality required by the Thread Child, Router, and Leader roles.
 *
 * @{
 *
 */

enum
{
    kVersion                    = 1,
    kUdpPort                    = 19788,
    kMaxChildren                = 5,
    kParentRequestRouterTimeout = 1000,  // milliseconds
    kParentRequestChildTimeout  = 2000,  // milliseconds
    kReedAdvertiseInterval      = 10,  // seconds
    kReedAdvertiseJitter        = 2,  // seconds
    kChildIdMask                = 0x1ff,
    kRouterIdOffset             = 10,
};

enum
{
    kAdvertiseIntervalMin       = 1,  // seconds
    kAdvertiseIntervalMax       = 32,  // seconds
    kRouterIdReuseDelay         = 100,  // seconds
    kRouterIdSequencePeriod     = 10,  // seconds
    kMaxNeighborAge             = 100,  // seconds
    kMaxRouteCost               = 16,
    kMaxRouterId                = 62,
    kMaxRouters                 = 32,
    kMinDowngradeNeighbors      = 7,
    kNetworkIdTimeout           = 120,  // seconds
    kParentRouteToLeaderTimeout = 20,  // seconds
    kRouterSelectionJitter      = 120,  // seconds
    kRouterDowngradeThreshold   = 23,
    kRouterUpgradeThreadhold    = 16,
    kMaxLeaderToRouterTimeout   = 90,  // seconds
};

enum
{
    kModeRxOnWhenIdle      = 1 << 3,
    kModeSecureDataRequest = 1 << 2,
    kModeFFD               = 1 << 1,
    kModeFullNetworkData   = 1 << 0,
};

enum DeviceState
{
    kDeviceStateDisabled = 0,
    kDeviceStateDetached = 1,
    kDeviceStateChild    = 2,
    kDeviceStateRouter   = 3,
    kDeviceStateLeader   = 4,
};

class Header
{
public:
    void Init() { mSecuritySuite = 0; mSecurityControl = Mac::Frame::kSecEncMic32; }
    bool IsValid() const {
        return mSecuritySuite == 0 &&
               (mSecurityControl == (Mac::Frame::kKeyIdMode1 | Mac::Frame::kSecEncMic32) ||
                mSecurityControl == (Mac::Frame::kKeyIdMode5 | Mac::Frame::kSecEncMic32));
    }

    uint8_t GetLength() const {
        return sizeof(mSecuritySuite) + sizeof(mSecurityControl) + sizeof(mFrameCounter) +
               (IsKeyIdMode1() ? 1 : 5) + sizeof(mCommand);
    }

    uint8_t GetHeaderLength() const {
        return sizeof(mSecurityControl) + sizeof(mFrameCounter) + (IsKeyIdMode1() ? 1 : 5);
    }

    const uint8_t *GetBytes() const {
        return reinterpret_cast<const uint8_t *>(&mSecuritySuite);
    }

    uint8_t GetSecurityControl() const { return mSecurityControl; }

    bool IsKeyIdMode1() const {
        return (mSecurityControl & Mac::Frame::kKeyIdModeMask) == Mac::Frame::kKeyIdMode1;
    }

    void SetKeyIdMode1() {
        mSecurityControl = (mSecurityControl & ~Mac::Frame::kKeyIdModeMask) | Mac::Frame::kKeyIdMode1;
    }

    void SetKeyIdMode2() {
        mSecurityControl = (mSecurityControl & ~Mac::Frame::kKeyIdModeMask) | Mac::Frame::kKeyIdMode5;
    }

    uint32_t GetKeyId() const {
        return IsKeyIdMode1() ? mKeyIdentifier[0] - 1 :
               static_cast<uint32_t>(mKeyIdentifier[3]) << 0 |
               static_cast<uint32_t>(mKeyIdentifier[2]) << 8 |
               static_cast<uint32_t>(mKeyIdentifier[1]) << 16 |
               static_cast<uint32_t>(mKeyIdentifier[0]) << 24;
    }

    void SetKeyId(uint32_t keySequence) {
        if (IsKeyIdMode1()) {
            mKeyIdentifier[0] = (keySequence & 0x7f) + 1;
        }
        else {
            mKeyIdentifier[4] = (keySequence & 0x7f) + 1;
            mKeyIdentifier[3] = keySequence >> 0;
            mKeyIdentifier[2] = keySequence >> 8;
            mKeyIdentifier[1] = keySequence >> 16;
            mKeyIdentifier[0] = keySequence >> 24;
        }
    }

    uint32_t GetFrameCounter() const {
        return Encoding::LittleEndian::HostSwap32(mFrameCounter);
    }

    void SetFrameCounter(uint32_t frameCounter) {
        mFrameCounter = Encoding::LittleEndian::HostSwap32(frameCounter);
    }

    enum Command
    {
        kCommandLinkRequest          = 0,
        kCommandLinkAccept           = 1,
        kCommandLinkAcceptAndRequest = 2,
        kCommandLinkReject           = 3,
        kCommandAdvertisement        = 4,
        kCommandUpdate               = 5,
        kCommandUpdateRequest        = 6,
        kCommandDataRequest          = 7,
        kCommandDataResponse         = 8,
        kCommandParentRequest        = 9,
        kCommandParentResponse       = 10,
        kCommandChildIdRequest       = 11,
        kCommandChildIdResponse      = 12,
        kCommandChildUpdateRequest   = 13,
        kCommandChildUpdateResponse  = 14,
    };
    Command GetCommand() const {
        const uint8_t *command = mKeyIdentifier + (IsKeyIdMode1() ? 1 : 5);
        return static_cast<Command>(*command);
    }

    void SetCommand(Command command) {
        uint8_t *commandField = mKeyIdentifier + (IsKeyIdMode1() ? 1 : 5);
        *commandField = static_cast<uint8_t>(command);
    }

    enum SecuritySuite
    {
        kSecurityEnabled  = 0x00,
        kSecurityDisabled = 0xff,
    };

private:
    uint8_t mSecuritySuite;
    uint8_t mSecurityControl;
    uint32_t mFrameCounter;
    uint8_t mKeyIdentifier[5];
    uint8_t mCommand;
} __attribute__((packed));

class Mle
{
public:
    explicit Mle(ThreadNetif &netif);
    ThreadError Init();
    ThreadError Start();
    ThreadError Stop();

    ThreadError BecomeDetached();
    ThreadError BecomeChild(otMleAttachFilter filter);

    DeviceState GetDeviceState() const;

    uint8_t GetDeviceMode() const;
    ThreadError SetDeviceMode(uint8_t mode);

    const uint8_t *GetMeshLocalPrefix() const;
    ThreadError SetMeshLocalPrefix(const uint8_t *prefix);

    const uint8_t GetChildId(uint16_t rloc16) const;
    const uint8_t GetRouterId(uint16_t rloc16) const;
    const uint16_t GetRloc16(uint8_t routerId) const;

    const Ip6Address *GetLinkLocalAllThreadNodesAddress() const;
    const Ip6Address *GetRealmLocalAllThreadNodesAddress() const;

    Router *GetParent();

    bool IsRoutingLocator(const Ip6Address &address) const;

    uint32_t GetTimeout() const;
    ThreadError SetTimeout(uint32_t timeout);

    uint16_t GetRloc16() const;
    const Ip6Address *GetMeshLocal16() const;
    const Ip6Address *GetMeshLocal64() const;

    ThreadError HandleNetworkDataUpdate();

    uint8_t GetLeaderId() const;
    ThreadError GetLeaderAddress(Ip6Address &address) const;
    const LeaderDataTlv *GetLeaderDataTlv();

protected:
    ThreadError AppendSecureHeader(Message &message, Header::Command command);
    ThreadError AppendSourceAddress(Message &message);
    ThreadError AppendMode(Message &message, uint8_t mode);
    ThreadError AppendTimeout(Message &message, uint32_t timeout);
    ThreadError AppendChallenge(Message &message, const uint8_t *challenge, uint8_t challengeLength);
    ThreadError AppendResponse(Message &message, const uint8_t *response, uint8_t responseLength);
    ThreadError AppendLinkFrameCounter(Message &message);
    ThreadError AppendMleFrameCounter(Message &message);
    ThreadError AppendAddress16(Message &message, uint16_t rloc16);
    ThreadError AppendNetworkData(Message &message, bool stable_only);
    ThreadError AppendTlvRequest(Message &message, const uint8_t *tlvs, uint8_t tlvsLength);
    ThreadError AppendLeaderData(Message &message);
    ThreadError AppendScanMask(Message &message, uint8_t scanMask);
    ThreadError AppendStatus(Message &message, StatusTlv::Status status);
    ThreadError AppendLinkMargin(Message &message, uint8_t linkMargin);
    ThreadError AppendVersion(Message &message);
    ThreadError AppendIp6Address(Message &message);
    ThreadError CheckReachability(Mac::Address16 meshsrc, Mac::Address16 meshdst, Ip6Header &ip6Header);
    void GenerateNonce(const Mac::Address64 &macAddr, uint32_t frameCounter, uint8_t securityLevel, uint8_t *nonce);
    Neighbor *GetNeighbor(const Mac::Address &address);
    Neighbor *GetNeighbor(Mac::Address16 address);
    Neighbor *GetNeighbor(const Mac::Address64 &address);
    Neighbor *GetNeighbor(const Ip6Address &address);
    Mac::Address16 GetNextHop(Mac::Address16 destination) const;
    static void HandleUnicastAddressesChanged(void *context);
    void HandleUnicastAddressesChanged();
    static void HandleParentRequestTimer(void *context);
    void HandleParentRequestTimer();
    static void HandleUdpReceive(void *context, otMessage message, const otMessageInfo *messageInfo);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &messageInfo);
    ThreadError HandleAdvertisement(const Message &message, const Ip6MessageInfo &messageInfo);
    ThreadError HandleDataRequest(const Message &message, const Ip6MessageInfo &messageInfo);
    ThreadError HandleDataResponse(const Message &message, const Ip6MessageInfo &messageInfo);
    ThreadError HandleParentResponse(const Message &message, const Ip6MessageInfo &messageInfo,
                                     uint32_t keySequence);
    ThreadError HandleChildIdResponse(const Message &message, const Ip6MessageInfo &messageInfo);
    ThreadError HandleChildUpdateResponse(const Message &message, const Ip6MessageInfo &messageInfo);
    uint8_t LinkMarginToQuality(uint8_t linkMargin);
    ThreadError SendParentRequest();
    ThreadError SendChildIdRequest();
    ThreadError SendDataRequest(const Ip6Address &destination, const uint8_t *tlvs, uint8_t tlvsLength);
    ThreadError SendDataResponse(const Ip6Address &destination, const uint8_t *tlvs, uint8_t tlvsLength);
    ThreadError SendChildUpdateRequest();
    ThreadError SendMessage(Message &message, const Ip6Address &destination);
    ThreadError SetRloc16(uint16_t rloc16);
    ThreadError SetStateDetached();
    ThreadError SetStateChild(uint16_t rloc16);

    NetifHandler mNetifHandler;
    Timer mParentRequestTimer;

    Udp6Socket mSocket;
    NetifUnicastAddress mLinkLocal16;
    NetifUnicastAddress mLinkLocal64;
    NetifUnicastAddress mMeshLocal64;
    NetifUnicastAddress mMeshLocal16;
    NetifMulticastAddress mLinkLocalAllThreadNodes;
    NetifMulticastAddress mRealmLocalAllThreadNodes;

    AddressResolver *mAddressResolver;
    KeyManager *mKeyManager;
    MeshForwarder *mMesh;
    MleRouter *mMleRouter;
    NetworkData::Leader *mNetworkData;
    ThreadNetif *mNetif;

    LeaderDataTlv mLeaderData;
    DeviceState mDeviceState = kDeviceStateDisabled;
    Router mParent;
    uint8_t mDeviceMode = kModeRxOnWhenIdle | kModeSecureDataRequest | kModeFFD | kModeFullNetworkData;
    uint32_t mTimeout = kMaxNeighborAge;

    enum ParentRequestState
    {
        kParentIdle,
        kParentSynchronize,
        kParentRequestStart,
        kParentRequestRouter,
        kParentRequestChild,
        kChildIdRequest,
    };
    ParentRequestState mParentRequestState = kParentIdle;
    otMleAttachFilter mParentRequestMode = kMleAttachAnyPartition;

    struct
    {
        uint8_t mChallenge[8];
    } mParentRequest;

    struct
    {
        uint8_t mChallenge[8];
        uint8_t mChallengeLength;
    } mChildIdRequest;

// used during the attach process
    uint32_t mParentConnectivity;
};

}  // namespace Mle

/**
 * @}
 */

}  // namespace Thread

#endif  // MLE_HPP_
