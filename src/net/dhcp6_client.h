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

#ifndef NET_DHCP6_CLIENT_H_
#define NET_DHCP6_CLIENT_H_

#include <mac/mac_frame.h>
#include <net/dhcp6.h>
#include <net/udp6.h>

namespace Thread {
namespace Dhcp6 {

class Dhcp6SolicitDelegate {
 public:
  virtual ThreadError HandleIaAddr(IaAddress *address) = 0;
  virtual ThreadError HandleVendorSpecificInformation(uint32_t enterprise_number, void *buf, uint16_t buf_length) = 0;
};

class Dhcp6LeaseQueryDelegate {
 public:
  virtual ThreadError HandleLeaseQueryReply(const Ip6Address *eid, const Ip6Address *rloc,
                                            uint32_t last_transaction_time) = 0;
};

class Dhcp6Client {
 public:
  explicit Dhcp6Client(Netif *netif);
  ThreadError Start();
  ThreadError Stop();
  ThreadError Solicit(const Ip6Address *dst, Dhcp6SolicitDelegate *delegate);
  ThreadError Release(const Ip6Address *dst);
  ThreadError LeaseQuery(const Ip6Address *target, Dhcp6LeaseQueryDelegate *delegate);
  bool HaveValidLease();
  ThreadError Reset();

 private:
  Udp6Socket socket_;
  uint8_t transaction_id_[3];
  Netif *netif_;
  Dhcp6SolicitDelegate *solicit_delegate_ = NULL;
  Dhcp6LeaseQueryDelegate *lease_query_delegate_ = NULL;
  IdentityAssociation identity_association_;
  uint8_t request_type_ = 0;

  static void RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info);
  void RecvFrom(Message *message, const Ip6MessageInfo *message_info);

  ThreadError AppendHeader(Message *message, uint8_t type);
  ThreadError AppendServerIdentifier(Message *message);
  ThreadError AppendClientIdentifier(Message *message);
  ThreadError AppendIaNa(Message *message, uint8_t type);
  ThreadError AppendElapsedTime(Message *message);
  ThreadError AppendOptionRequest(Message *message);
  ThreadError AppendRapidCommit(Message *message);
  ThreadError AppendLeaseQuery(Message *message, const Ip6Address *target);

  void ProcessReply(Message *message, const Ip6MessageInfo *address);
  void ProcessLeaseQueryReply(Message *message, const Ip6MessageInfo *address);
  uint16_t FindOption(Message *message, uint16_t offset, uint16_t length, uint16_t type);
  ThreadError ProcessClientIdentifier(Message *message, uint16_t offset);
  ThreadError ProcessServerIdentifier(Message *message, uint16_t offset, ServerIdentifier *server_identifier);
  ThreadError ProcessIaNa(Message *message, uint16_t offset);
  ThreadError ProcessIaAddr(Message *message, uint16_t offset);
  ThreadError ProcessStatusCode(Message *message, uint16_t offset);
  ThreadError ProcessVendorSpecificInformation(Message *message, uint16_t offset);
  ThreadError ProcessClientData(Message *message, uint16_t offset);
};

}  // namespace Dhcp6
}  // namespace Thread

#endif  // NET_DHCP6_CLIENT_H_
