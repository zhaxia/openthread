/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#ifndef THREAD_MLE_H_
#define THREAD_MLE_H_

#include <common/timer.h>
#include <crypto/aes_ecb.h>
#include <net/udp6.h>
#include <thread/address_resolver.h>
#include <thread/key_manager.h>
#include <thread/mesh_forwarder.h>
#include <thread/network_data_leader.h>
#include <thread/topology.h>

namespace Thread {

#define ADDR16_TO_ROUTER_ID(x) (static_cast<uint8_t>((x) >> kRouterIdOffset))
#define ADDR16_TO_CHILD_ID(x) (static_cast<uint16_t>((x) & kChildIdMask))
#define ROUTER_ID_TO_ADDR16(x) (static_cast<uint16_t>(x) << kRouterIdOffset)

class ThreadNetif;

class Mle {
 public:
  enum {
    kVersion                       = 1,
    kUdpPort                       = 19788,
    kMaxChildren                   = 5,
    kParentRequestRouterTimeout    = 1000,  // milliseconds
    kParentRequestChildTimeout     = 2000,  // milliseconds
    kReedAdvertiseInterval         = 10,  // seconds
    kReedAdvertiseJitter           = 2,  // seconds
  };

  enum {
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

  enum {
    kModeRxOnWhenIdle      = 1 << 3,
    kModeSecureDataRequest = 1 << 2,
    kModeFFD               = 1 << 1,
    kModeFullNetworkData   = 1 << 0,
  };

  enum DeviceState {
    kDeviceStateDisabled = 0,
    kDeviceStateDetached = 1,
    kDeviceStateChild    = 2,
    kDeviceStateRouter   = 3,
    kDeviceStateLeader   = 4,
  };

  enum JoinMode {
    kJoinAnyPartition    = 0,
    kJoinSamePartition   = 1,
    kJoinBetterPartition = 2,
  };

  struct LeaderData {
    uint32_t partition_id;
    uint8_t weighting;
    uint8_t version;
    uint8_t stable_version;
    uint8_t leader_router_id;
  } __attribute__((packed));

  Mle(AddressResolver *address_resolver, KeyManager *key_manager, MeshForwarder *mesh, MleRouter *mle_router,
      ThreadNetif *netif, NetworkDataLeader *network_data);
  ThreadError Init();
  ThreadError Start();
  ThreadError Stop();

  ThreadError BecomeDetached();
  ThreadError BecomeChild(JoinMode mode);

  DeviceState GetDeviceState() const;
  ThreadError SetStateDetached();
  ThreadError SetStateChild(uint16_t address16);

  uint8_t GetDeviceMode() const;
  ThreadError SetDeviceMode(uint8_t mode);

  const uint8_t *GetMeshLocalPrefix() const;
  ThreadError SetMeshLocalPrefix(const uint8_t *prefix);

  const Ip6Address *GetLinkLocalAllThreadNodesAddress() const;
  const Ip6Address *GetRealmLocalAllThreadNodesAddress() const;

  Router *GetParent();

  bool IsRoutingLocator(const Ip6Address *address);

  uint32_t GetTimeout() const;
  ThreadError SetTimeout(uint32_t timeout);

  uint16_t GetAddress16() const;
  const Ip6Address *GetMeshLocal16();
  const Ip6Address *GetMeshLocal64();

  ThreadError HandleNetworkDataUpdate();

  uint8_t GetLeaderId() const;
  ThreadError GetLeaderAddress(Ip6Address *address) const;
  LeaderData *GetLeaderData();

  ThreadError AppendSecureHeader(Message *message, uint8_t command);
  ThreadError AppendSourceAddress(Message *message);
  ThreadError AppendMode(Message *message, uint8_t mode);
  ThreadError AppendTimeout(Message *message, uint32_t timeout);
  ThreadError AppendChallenge(Message *message, uint8_t *challenge, uint8_t challenge_length);
  ThreadError AppendResponse(Message *message, const uint8_t *response, uint8_t response_length);
  ThreadError AppendLinkFrameCounter(Message *message);
  ThreadError AppendMleFrameCounter(Message *message);
  ThreadError AppendAddress16(Message *message, uint16_t address16);
  ThreadError AppendNetworkData(Message *message, bool stable_only);
  ThreadError AppendTlvRequest(Message *message, const uint8_t *tlvs, uint8_t tlvs_length);
  ThreadError AppendLeaderData(Message *message);
  ThreadError AppendScanMask(Message *message, uint8_t scan_mask);
  ThreadError AppendStatus(Message *message, uint8_t status);
  ThreadError AppendRssi(Message *message, int8_t rssi);
  ThreadError AppendVersion(Message *message);
  ThreadError AppendIp6Address(Message *message);
  ThreadError AppendIp6Address(Message *message, Child *child);
  ThreadError FindTlv(Message *message, uint8_t type, void *buf, uint16_t buf_length);
  ThreadError SendMessage(Message *message, const Ip6Address *destination);

 protected:
  struct Header {
    enum {
      kSecurityEnabled  = 0x00,
      kSecurityDisabled = 0xff,
    };
    uint8_t security_suite;
    uint8_t security_control;
    uint32_t frame_counter;
    uint8_t key_identifier[9];

    enum {
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
  } __attribute__((packed));

  struct Tlv {
    enum {
      kTypeSourceAddress    = 0,
      kTypeMode             = 1,
      kTypeTimeout          = 2,
      kTypeChallenge        = 3,
      kTypeResponse         = 4,
      kTypeLinkFrameCounter = 5,
      kTypeLinkQuality      = 6,
      kTypeNetworkParameter = 7,
      kTypeMleFrameCounter  = 8,
      kTypeRoute            = 9,
      kTypeAddress16        = 10,
      kTypeLeaderData       = 11,
      kTypeNetworkData      = 12,
      kTypeTlvRequest       = 13,
      kTypeScanMask         = 14,
      kTypeConnectivity     = 15,
      kTypeRssi             = 16,
      kTypeStatus           = 17,
      kTypeVersion          = 18,
      kTypeIp6Address       = 19,
      kTypeHoldTime         = 20,
      kTypeInvalid          = 255,
    };
    uint8_t type;
    uint8_t length;
  } __attribute__((packed));

  struct SourceAddressTlv {
    Tlv header;
    uint16_t address16;
  } __attribute__((packed));

  struct ModeTlv {
    Tlv header;
    uint8_t mode;
  } __attribute__((packed));

  struct TimeoutTlv {
    Tlv header;
    uint32_t timeout;
  } __attribute__((packed));

  struct ChallengeTlv {
    Tlv header;
    uint8_t challenge[8];
  } __attribute__((packed));

  struct ResponseTlv {
    Tlv header;
    uint8_t response[8];
  } __attribute__((packed));

  struct LinkFrameCounterTlv {
    Tlv header;
    uint32_t frame_counter;
  } __attribute__((packed));

  struct RouteTlv {
    Tlv header;
    uint8_t sequence;
    uint8_t router_mask[(kMaxRouterId+7)/8];
    uint8_t route_data[kMaxRouterId];
  } __attribute__((packed));

  struct MleFrameCounterTlv {
    Tlv header;
    uint32_t frame_counter;
  } __attribute__((packed));

  struct Address16Tlv {
    Tlv header;
    uint16_t address16;
  } __attribute__((packed));

  struct LeaderDataTlv {
    Tlv header;
    LeaderData leader_data;
  } __attribute__((packed));

  struct NetworkDataTlv {
    Tlv header;
    uint8_t network_data[255];
  } __attribute__((packed));

  struct TlvRequestTlv {
    Tlv header;
    uint8_t tlvs[8];
  } __attribute__((packed));

  struct ScanMaskTlv {
    Tlv header;
    enum {
      kRouterScan = 1 << 7,
      kChildScan = 1 << 6,
    };
    uint8_t scan_mask;
  } __attribute__((packed));

  struct ConnectivityTlv {
    Tlv header;
    uint8_t max_child_count;
    uint8_t child_count;
    uint8_t link_quality_3;
    uint8_t link_quality_2;
    uint8_t link_quality_1;
    uint8_t leader_cost;
    uint8_t id_sequence;
  } __attribute__((packed));

  struct RssiTlv {
    Tlv header;
    uint8_t rssi;
  } __attribute__((packed));

  struct StatusTlv {
    Tlv header;
    enum {
      kError = 1,
    };
    uint8_t status;
  } __attribute__((packed));

  struct VersionTlv {
    Tlv header;
    uint16_t version;
  } __attribute__((packed));

  struct AddressRegistrationEntry {
    enum {
      kCompressed = 1 << 7,
      kContextIdMask = 0xf,
    };
    uint8_t control;
    union {
      uint8_t iid[8];
      Ip6Address ip6_address;
    };
  } __attribute__((packed));

  struct Ip6AddressTlv {
    Tlv header;
    AddressRegistrationEntry address[Child::kMaxIp6AddressPerChild + 1];  // one more to account for Mesh Local
  } __attribute__((packed));

  ThreadError SetAddress16(uint16_t address16);

  void GenerateNonce(const MacAddr64 *mac_addr, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce);
  ThreadError SendParentRequest();
  ThreadError SendChildIdRequest();
  ThreadError SendDataRequest(const Ip6MessageInfo *message_info, uint8_t *tlvs, uint8_t tlvs_length);
  ThreadError SendDataResponse(const Ip6Address *destination, const uint8_t *tlvs, uint8_t tlvs_length);
  ThreadError SendChildUpdateRequest();

  static void RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info);
  void RecvFrom(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleAdvertisement(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleDataRequest(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleDataResponse(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleParentResponse(Message *message, const Ip6MessageInfo *message_info, uint32_t key_sequence);
  ThreadError HandleChildIdResponse(Message *message, const Ip6MessageInfo *message_info);
  ThreadError HandleChildUpdateResponse(Message *message, const Ip6MessageInfo *message_info);

  Child *GetChild(MacAddr16 address);
  Child *GetChild(const MacAddr64 *address);
  Child *GetChild(const MacAddress *address);
  int GetChildIndex(const Child *child);
  Child *GetChildren(uint8_t *num_children);
  Neighbor *GetNeighbor(const MacAddress *address);
  Neighbor *GetNeighbor(MacAddr16 address);
  Neighbor *GetNeighbor(const MacAddr64 *address);
  Neighbor *GetNeighbor(const Ip6Address *address);
  MacAddr16 GetNextHop(MacAddr16 destination);
  ThreadError CheckReachability(MacAddr16 meshsrc, MacAddr16 meshdst, const Ip6Header *ip6_header);
  uint8_t LinkMarginToQuality(uint8_t link_margin);

  static void HandleNetifCallback(void *context);
  void HandleNetifCallback();

  static void HandleParentRequestTimer(void *context);
  void HandleParentRequestTimer();

  NetifCallback netif_callback_;
  Timer parent_request_timer_;

  Udp6Socket socket_;
  NetifAddress link_local_16_;
  NetifAddress link_local_64_;
  NetifAddress mesh_local_64_;
  NetifAddress mesh_local_16_;
  NetifMulticastAddress link_local_all_thread_nodes_;
  NetifMulticastAddress realm_local_all_thread_nodes_;

  AddressResolver *address_resolver_;
  KeyManager *key_manager_;
  Mac *mac_;
  MeshForwarder *mesh_;
  MleRouter *mle_router_;
  NetworkDataLeader *network_data_;
  ThreadNetif *netif_;

  LeaderData leader_data_;
  DeviceState device_state_ = kDeviceStateDisabled;
  Router parent_;
  uint8_t device_mode_ = kModeRxOnWhenIdle | kModeSecureDataRequest | kModeFFD | kModeFullNetworkData;
  uint32_t timeout_ = kMaxNeighborAge;

  enum {
    kParentIdle,
    kParentSynchronize,
    kParentRequestStart,
    kParentRequestRouter,
    kParentRequestChild,
    kChildIdRequest,
  };
  uint8_t parent_request_state_ = kParentIdle;
  JoinMode parent_request_mode_ = kJoinAnyPartition;

  struct {
    uint8_t challenge[8];
  } parent_request_;

  struct {
    uint8_t challenge[8];
    uint8_t challenge_length;
  } child_id_request_;

  // used during the attach process
  uint32_t parent_connectivity_;
};

}  // namespace Thread

#endif  // THREAD_MLE_H_
