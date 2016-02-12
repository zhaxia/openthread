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

#ifndef THREAD_THREAD_TLVS_H_
#define THREAD_THREAD_TLVS_H_

#include <common/message.h>
#include <mac/mac_frame.h>
#include <net/ip6_address.h>
#include <thread/mle.h>

namespace Thread {

enum {
  kCoapUdpPort = 19789,
};

struct ThreadTlv {
  enum {
    kTypeTargetEid           = 0,
    kTypeMacAddr64           = 1,
    kTypeRloc                = 2,
    kTypeMeshLocalIid        = 3,
    kTypeStatus              = 4,
    kTypeLastTransactionTime = 6,
    kTypeRouterMask          = 7,
  };
  uint8_t type;
  uint8_t length;
} __attribute__((packed));

struct ThreadTargetEidTlv {
  ThreadTlv header;
  Ip6Address address;
} __attribute__((packed));

struct ThreadMacAddr64Tlv {
  ThreadTlv header;
  MacAddr64 address;
} __attribute__((packed));

struct ThreadRlocTlv {
  ThreadTlv header;
  uint16_t address;
} __attribute__((packed));

struct ThreadMeshLocalIidTlv {
  ThreadTlv header;
  uint8_t iid[8];
} __attribute__((packed));

struct ThreadStatusTlv {
  ThreadTlv header;
  enum {
    kSuccess = 0,
    kNoAddressAvailable = 1,
  };
  uint8_t status;
} __attribute__((packed));

struct ThreadLastTransactionTimeTlv {
  ThreadTlv header;
  uint32_t time;
} __attribute__((packed));

struct ThreadRouterMaskTlv {
  ThreadTlv header;
  uint8_t router_id_sequence;
  uint8_t router_id_mask[(Mle::kMaxRouterId+7)/8];
};

}  // namespace Thread

#endif  // THREAD_THREAD_TLVS_H_
