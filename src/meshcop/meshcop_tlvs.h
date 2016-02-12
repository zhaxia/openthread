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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#ifndef THREAD_MESHCOP_TLVS_H_
#define THREAD_MESHCOP_TLVS_H_

#include <common/message.h>
#include <mac/mac_frame.h>
#include <net/ip6_address.h>
#include <thread/mle.h>

namespace Thread {

enum {
  kCoapDefaultCommPort = 19779,
  kCoapDefaultFwdPort = 19782,
  kCoapDefaultJoinPort = 19786,
};

struct MeshCopTlv {
  enum {
    kTypeChannel                 = 0,
    kTypePanId                   = 1,
    kTypeXPanId                  = 2,
    kTypeNetworkName             = 3,
    kTypePSKc                    = 4,
    kTypeMasterKey               = 5,
    kTypeKeySequence             = 6,
    kTypeMeshPrefix              = 7,
    kTypeSteeringData            = 8,
    kTypeBorderRloc              = 9,
    kTypeCommissionerId          = 10,
    kTypeCommissionerSessionId   = 11,
    kTypeCommissionerDataset     = 14,
    kTypeCommissionerUdpPort     = 15,
    kTypeSecurityPolicy          = 12,

    kTypeGet                     = 13,
    kTypeState                   = 16,
    kTypeJoinerDtls              = 17,
    kTypeJoinerUdpPort           = 18,
    kTypeJoinerIid               = 19,
    kTypeJoinerRloc              = 20,
    kTypeJoinerRouterKek         = 21,

    kTypeProvisioningUrl         = 32,
    kTypeVendorName              = 33,
    kTypeVendorModel             = 34,
    kTypeVendorSwVersion         = 35,
    kTypeVendorData              = 36,
    kTypeVendorStackVersion      = 37,

    kTypeUdpEncapsulation        = 48,
    kTypeIpv6Address             = 49,
    kTypeTmfForwardingPort       = 50,
  };
  uint8_t type;
  uint8_t length;
} __attribute__((packed));

struct ThreadChannelTlv {
  ThreadTlv header;
  uint16_t channel;
} __attribute__((packed));

struct ThreadPanIdTlv {
  ThreadTlv header;
  uint16_t panid;
} __attribute__((packed));

struct ThreadXPanIdTlv {
  ThreadTlv header;
  uint8_t xpanid[8];
} __attribute__((packed));

struct ThreadNetworkNameTlv {
  ThreadTlv header;
  uint8_t name[16];
} __attribute__((packed));

struct ThreadPSKcTlv {
  ThreadTlv header;
  uint8_t pskc[16];
} __attribute__((packed));

struct ThreadMasterKeyTlv {
  ThreadTlv header;
  uint8_t master_key[16];
} __attribute__((packed));

struct ThreadKeySequenceTlv {
  ThreadTlv header;
  uint32_t key_seq;
} __attribute__((packed));

struct ThreadMeshPrefixTlv {
  ThreadTlv header;
  uint8_t mesh_ula[8];
} __attribute__((packed));

struct ThreadSteeringDataTlv {
  ThreadTlv header;
  uint8_t bloom[8];
} __attribute__((packed));

struct ThreadBorderRlocTlv {
  ThreadTlv header;
  uint16_t address;
} __attribute__((packed));

struct ThreadCommissionerIdTlv {
  ThreadTlv header;
  uint8_t id[64];
} __attribute__((packed));

struct ThreadCommissionerSessionIdTlv {
  ThreadTlv header;
  uint16_t session;
} __attribute__((packed));

struct ThreadCommissioningDatasetTimestampTlv {
  ThreadTlv header;
  uint64_t timestamp;
} __attribute__((packed));

struct ThreadCommissionerUdpPortTlv {
  ThreadTlv header;
  uint16_t port;
} __attribute__((packed));

struct ThreadSecurityPolicyTlv {
  ThreadTlv header;
  struct {
    uint8_t rsvd:6;
    uint8_t n:1;
    uint8_t o:1;
  } bits;
  uint16_t rotation_time;
} __attribute__((packed));




struct ThreadStateTlv {
  ThreadTlv header;
  enum {
    kAccept = 1,
    kPending = 0,
    kReject = -1,
  };
  uint8_t status;
} __attribute__((packed));

struct ThreadJoinerIidTlv {
  ThreadTlv header;
  uint8_t iid[8];
} __attribute__((packed));

struct ThreadJoinerUdpPortTlv {
  ThreadTlv header;
  uint16_t port;
} __attribute__((packed));

struct ThreadJoinerIidTlv {
  ThreadTlv header;
  uint8_t iid[8];
} __attribute__((packed));

struct ThreadJoinerRlocTlv {
  ThreadTlv header;
  uint16_t address;
} __attribute__((packed));

struct ThreadJoinerRouterKekTlv {
  ThreadTlv header;
  uint8_t kek[16];
} __attribute__((packed));

struct ThreadProvisioningUrlTlv {
  ThreadTlv header;
  uint8_t url[64];
} __attribute__((packed));

struct ThreadVendorNameTlv {
  ThreadTlv header;
  uint8_t url[32];
} __attribute__((packed));

struct ThreadVendorModelTlv {
  ThreadTlv header;
  uint8_t url[32];
} __attribute__((packed));

struct ThreadVendorSwVersion {
  ThreadTlv header;
  uint8_t version[16];
} __attribute__((packed));

struct ThreadVendorDataTlv {
  ThreadTlv header;
  uint8_t url[64];
} __attribute__((packed));

struct ThreadVendorStackVersionTlv {
  ThreadTlv header;
  uint8_t oui[3];
  uint16_t build;
  uint8_t version;
} __attribute__((packed));

struct ThreadUdpEncapsulationTlv {
  ThreadTlv header;
  uint16_t src_port;
  uint16_t dst_port;
  uint8_t payload[128];
} __attribute__((packed));

struct ThreadIpv6AddressTlv {
  ThreadTlv header;
  uint8_t address[16];
} __attribute__((packed));

struct ThreadForwardingUdpPortTlv {
  ThreadTlv header;
  uint16_t port;
} __attribute__((packed));

}  // namespace Thread

#endif  // THREAD_THREAD_TLVS_H_
