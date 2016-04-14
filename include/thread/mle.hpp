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

#ifndef MLE_HPP_
#define MLE_HPP_

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

namespace Mle {

class MleRouter;

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

enum JoinMode
{
    kJoinAnyPartition    = 0,
    kJoinSamePartition   = 1,
    kJoinBetterPartition = 2,
};

class Header
{
public:
    void Init() { m_security_suite = 0; m_security_control = Mac::Frame::kSecEncMic32; }
    bool IsValid() const {
        return m_security_suite == 0 &&
               (m_security_control == (Mac::Frame::kKeyIdMode1 | Mac::Frame::kSecEncMic32) ||
                m_security_control == (Mac::Frame::kKeyIdMode5 | Mac::Frame::kSecEncMic32));
    }

    uint8_t GetLength() const {
        return sizeof(m_security_suite) + sizeof(m_security_control) + sizeof(m_frame_counter) +
               (IsKeyIdMode1() ? 1 : 5) + sizeof(m_command);
    }

    uint8_t GetHeaderLength() const {
        return sizeof(m_security_control) + sizeof(m_frame_counter) + (IsKeyIdMode1() ? 1 : 5);
    }

    const uint8_t *GetBytes() const {
        return reinterpret_cast<const uint8_t *>(&m_security_suite);
    }

    uint8_t GetSecurityControl() const { return m_security_control; }

    bool IsKeyIdMode1() const {
        return (m_security_control & Mac::Frame::kKeyIdModeMask) == Mac::Frame::kKeyIdMode1;
    }

    void SetKeyIdMode1() {
        m_security_control = (m_security_control & ~Mac::Frame::kKeyIdModeMask) | Mac::Frame::kKeyIdMode1;
    }

    void SetKeyIdMode2() {
        m_security_control = (m_security_control & ~Mac::Frame::kKeyIdModeMask) | Mac::Frame::kKeyIdMode5;
    }

    uint32_t GetKeyId() const {
        return IsKeyIdMode1() ? m_key_identifier[0] - 1 :
               static_cast<uint32_t>(m_key_identifier[3]) << 0 |
               static_cast<uint32_t>(m_key_identifier[2]) << 8 |
               static_cast<uint32_t>(m_key_identifier[1]) << 16 |
               static_cast<uint32_t>(m_key_identifier[0]) << 24;
    }

    void SetKeyId(uint32_t key_sequence) {
        if (IsKeyIdMode1()) {
            m_key_identifier[0] = (key_sequence & 0x7f) + 1;
        }
        else {
            m_key_identifier[4] = (key_sequence & 0x7f) + 1;
            m_key_identifier[3] = key_sequence >> 0;
            m_key_identifier[2] = key_sequence >> 8;
            m_key_identifier[1] = key_sequence >> 16;
            m_key_identifier[0] = key_sequence >> 24;
        }
    }

    uint32_t GetFrameCounter() const {
        return Encoding::LittleEndian::HostSwap32(m_frame_counter);
    }

    void SetFrameCounter(uint32_t frame_counter) {
        m_frame_counter = Encoding::LittleEndian::HostSwap32(frame_counter);
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
        const uint8_t *command = m_key_identifier + (IsKeyIdMode1() ? 1 : 5);
        return static_cast<Command>(*command);
    }

    void SetCommand(Command command) {
        uint8_t *command_field = m_key_identifier + (IsKeyIdMode1() ? 1 : 5);
        *command_field = static_cast<uint8_t>(command);
    }

    enum SecuritySuite
    {
        kSecurityEnabled  = 0x00,
        kSecurityDisabled = 0xff,
    };

private:
    uint8_t m_security_suite;
    uint8_t m_security_control;
    uint32_t m_frame_counter;
    uint8_t m_key_identifier[5];
    uint8_t m_command;
} __attribute__((packed));

class Mle
{
public:
    explicit Mle(ThreadNetif &netif);
    ThreadError Init();
    ThreadError Start();
    ThreadError Stop();

    ThreadError BecomeDetached();
    ThreadError BecomeChild(JoinMode mode);

    DeviceState GetDeviceState() const;

    uint8_t GetDeviceMode() const;
    ThreadError SetDeviceMode(uint8_t mode);

    const uint8_t *GetMeshLocalPrefix() const;
    ThreadError SetMeshLocalPrefix(const uint8_t *prefix);

    const uint8_t GetChildId(uint16_t rloc16) const;
    const uint8_t GetRouterId(uint16_t rloc16) const;
    const uint16_t GetRloc16(uint8_t router_id) const;

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
    ThreadError AppendChallenge(Message &message, const uint8_t *challenge, uint8_t challenge_length);
    ThreadError AppendResponse(Message &message, const uint8_t *response, uint8_t response_length);
    ThreadError AppendLinkFrameCounter(Message &message);
    ThreadError AppendMleFrameCounter(Message &message);
    ThreadError AppendAddress16(Message &message, uint16_t rloc16);
    ThreadError AppendNetworkData(Message &message, bool stable_only);
    ThreadError AppendTlvRequest(Message &message, const uint8_t *tlvs, uint8_t tlvs_length);
    ThreadError AppendLeaderData(Message &message);
    ThreadError AppendScanMask(Message &message, uint8_t scan_mask);
    ThreadError AppendStatus(Message &message, StatusTlv::Status status);
    ThreadError AppendLinkMargin(Message &message, uint8_t link_margin);
    ThreadError AppendVersion(Message &message);
    ThreadError AppendIp6Address(Message &message);
    ThreadError CheckReachability(Mac::Address16 meshsrc, Mac::Address16 meshdst, Ip6Header &ip6_header);
    void GenerateNonce(const Mac::Address64 &mac_addr, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce);
    Neighbor *GetNeighbor(const Mac::Address &address);
    Neighbor *GetNeighbor(Mac::Address16 address);
    Neighbor *GetNeighbor(const Mac::Address64 &address);
    Neighbor *GetNeighbor(const Ip6Address &address);
    Mac::Address16 GetNextHop(Mac::Address16 destination) const;
    static void HandleUnicastAddressesChanged(void *context);
    void HandleUnicastAddressesChanged();
    static void HandleParentRequestTimer(void *context);
    void HandleParentRequestTimer();
    static void HandleUdpReceive(void *context, Message &message, const Ip6MessageInfo &message_info);
    void HandleUdpReceive(Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleAdvertisement(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleDataRequest(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleDataResponse(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleParentResponse(const Message &message, const Ip6MessageInfo &message_info,
                                     uint32_t key_sequence);
    ThreadError HandleChildIdResponse(const Message &message, const Ip6MessageInfo &message_info);
    ThreadError HandleChildUpdateResponse(const Message &message, const Ip6MessageInfo &message_info);
    uint8_t LinkMarginToQuality(uint8_t link_margin);
    ThreadError SendParentRequest();
    ThreadError SendChildIdRequest();
    ThreadError SendDataRequest(const Ip6Address &destination, const uint8_t *tlvs, uint8_t tlvs_length);
    ThreadError SendDataResponse(const Ip6Address &destination, const uint8_t *tlvs, uint8_t tlvs_length);
    ThreadError SendChildUpdateRequest();
    ThreadError SendMessage(Message &message, const Ip6Address &destination);
    ThreadError SetRloc16(uint16_t rloc16);
    ThreadError SetStateDetached();
    ThreadError SetStateChild(uint16_t rloc16);

    NetifHandler m_netif_handler;
    Timer m_parent_request_timer;

    Udp6Socket m_socket;
    NetifUnicastAddress m_link_local_16;
    NetifUnicastAddress m_link_local_64;
    NetifUnicastAddress m_mesh_local_64;
    NetifUnicastAddress m_mesh_local_16;
    NetifMulticastAddress m_link_local_all_thread_nodes;
    NetifMulticastAddress m_realm_local_all_thread_nodes;

    AddressResolver *m_address_resolver;
    KeyManager *m_key_manager;
    MeshForwarder *m_mesh;
    MleRouter *m_mle_router;
    NetworkData::Leader *m_network_data;
    ThreadNetif *m_netif;

    LeaderDataTlv m_leader_data;
    DeviceState m_device_state = kDeviceStateDisabled;
    Router m_parent;
    uint8_t m_device_mode = kModeRxOnWhenIdle | kModeSecureDataRequest | kModeFFD | kModeFullNetworkData;
    uint32_t m_timeout = kMaxNeighborAge;

    enum ParentRequestState
    {
        kParentIdle,
        kParentSynchronize,
        kParentRequestStart,
        kParentRequestRouter,
        kParentRequestChild,
        kChildIdRequest,
    };
    ParentRequestState m_parent_request_state = kParentIdle;
    JoinMode m_parent_request_mode = kJoinAnyPartition;

    struct
    {
        uint8_t challenge[8];
    } m_parent_request;

    struct
    {
        uint8_t challenge[8];
        uint8_t challenge_length;
    } m_child_id_request;

    // used during the attach process
    uint32_t m_parent_connectivity;
};

}  // namespace Mle
}  // namespace Thread

#endif  // MLE_HPP_
