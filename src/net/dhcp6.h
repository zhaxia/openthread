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

#ifndef NET_DHCP6_H_
#define NET_DHCP6_H_

#include <common/message.h>
#include <net/udp6.h>

namespace Thread {
namespace Dhcp6 {

enum {
  kUdpClientPort = 546,
  kUdpServerPort = 547,
};

enum {
  kTypeSolicit = 1,
  kTypeAdvertise = 2,
  kTypeRequest = 3,
  kTypeConfirm = 4,
  kTypeRenew = 5,
  kTypeRebind = 6,
  kTypeReply = 7,
  kTypeRelease = 8,
  kTypeDecline = 9,
  kTypeReconfigure = 10,
  kTypeInformationRequest = 11,
  kTypeRelayForward = 12,
  kTypeRelayReply = 13,
  kTypeLeaseQuery = 14,
  kTypeLeaseQueryReply = 15,
};

struct Dhcp6Header {
  uint8_t type;
  uint8_t transaction_id[3];
};

enum {
  kOptionClientIdentifier = 1,
  kOptionServerIdentifier = 2,
  kOptionIaNa = 3,
  kOptionIaTa = 4,
  kOptionIaAddress = 5,
  kOptionRequestOption = 6,
  kOptionPreference = 7,
  kOptionElapsedTime = 8,
  kOptionRelayMessage = 9,
  kOptionAuthentication = 11,
  kOptionServerUnicast = 12,
  kOptionStatusCode = 13,
  kOptionRapidCommit = 14,
  kOptionUserClass = 15,
  kOptionVendorClass = 16,
  kOptionVendorSpecificInformation = 17,
  kOptionInterfaceId = 18,
  kOptionReconfigureMessage = 19,
  kOptionReconfigureAccept = 20,
  kOptionLeaseQuery = 44,
  kOptionClientData = 45,
  kOptionClientLastTransactionTime = 46,
};

struct Dhcp6Option {
  uint16_t code;
  uint16_t length;
} __attribute__((packed));

struct ClientIdentifier {
  Dhcp6Option header;
  uint16_t duid_type;
  uint16_t duid_hardware_type;
  uint8_t  duid_eui64[8];
} __attribute__((packed));

struct ServerIdentifier {
  Dhcp6Option header;
  uint16_t duid_type;
  uint16_t duid_hardware_type;
  uint8_t  duid_eui64[8];
} __attribute__((packed));

struct IaNa {
  Dhcp6Option header;
  uint32_t iaid;
  uint32_t t1;
  uint32_t t2;
} __attribute__((packed));

struct IaAddress {
  Dhcp6Option header;
  Ip6Address address;
  uint32_t preferred_lifetime;
  uint32_t valid_lifetime;
} __attribute__((packed));

struct OptionRequest {
  Dhcp6Option header;
  uint16_t options;
} __attribute__((packed));

struct ElapsedTime {
  Dhcp6Option header;
  uint16_t elapsed_time;
} __attribute__((packed));

enum {
  kStatusSuccess = 0,
  kStatusUnspecFail = 1,
  kStatusNoAddrsAvail = 2,
  kStatusNoBinding = 3,
  kStatusNotOnLink = 4,
  kStatusUseMulticast = 5,
  kUnknownQueryType = 7,
  kMalformedQuery = 8,
  kNotConfigured = 9,
  kNotAllowed = 10,
};

struct StatusCode {
  Dhcp6Option header;
  uint16_t status_code;
} __attribute__((packed));

struct RapidCommit {
  Dhcp6Option header;
} __attribute__((packed));

struct VendorSpecificInformation {
  Dhcp6Option header;
  uint32_t enterprise_number;
} __attribute__((packed));

enum {
  kDuidLinkLayerAddressPlusTime = 1,
  kDuidVendorBased = 2,
  kDuidLinkLayerAddress = 3,
};

enum {
  kHardwareTypeEui64 = 27
};

struct IdentityAssociation {
  ServerIdentifier server;
  IaNa ia_na;
  IaAddress ia_address;
} __attribute__((packed));

enum {
  kQueryByAddress = 1,
  kQueryByClientId = 2,
};

struct LeaseQueryOption {
  Dhcp6Option header;
  uint8_t query_type;
  Ip6Address link_address;
  IaAddress ia_address;
} __attribute__((packed));

struct ClientData {
  Dhcp6Option header;
} __attribute__((packed));

struct ClientLastTransactionTime {
  Dhcp6Option header;
  uint32_t last_transaction_time;
} __attribute__((packed));

}  // namespace Dhcp6
}  // namespace Thread

#endif  // NET_DHCP6_H_
