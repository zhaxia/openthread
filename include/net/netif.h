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

#ifndef NET_NETIF_H_
#define NET_NETIF_H_

#include <common/message.h>
#include <common/tasklet.h>
#include <mac/mac_frame.h>
#include <net/ip6_address.h>
#include <net/socket.h>

namespace Thread {

class LinkAddress {
 public :
  enum HardwareType {
    kEui64 = 27,
  };
  HardwareType type;
  uint8_t length;
  MacAddr64 address64;
};

class NetifAddress {
 public:
  Ip6Address address;
  uint32_t preferred_lifetime;
  uint32_t valid_lifetime;
  uint8_t prefix_length;
  NetifAddress *next_;
};

class NetifMulticastAddress {
 public:
  Ip6Address address_;
  NetifMulticastAddress *next_;
};

class NetifCallback {
  friend class Netif;

 public:
  typedef void (*Callback)(void *context);
  NetifCallback(Callback callback, void *context) {
    callback_ = callback;
    context_ = context;
  }

 private:
  Callback callback_;
  void *context_;
  NetifCallback *next_;
};

class Netif {
 public:
  Netif();
  ThreadError AddNetif();
  ThreadError RemoveNetif();
  Netif *GetNext() const;
  int GetInterfaceId() const;
  const NetifAddress *GetAddresses() const;
  ThreadError AddAddress(NetifAddress *address);
  ThreadError RemoveAddress(const NetifAddress *address);

  bool IsMulticastSubscribed(Ip6Address *address) const;
  ThreadError SubscribeAllRoutersMulticast();
  ThreadError UnsubscribeAllRoutersMulticast();
  ThreadError SubscribeMulticast(NetifMulticastAddress *address);
  ThreadError UnsubscribeMulticast(NetifMulticastAddress *address);

  ThreadError RegisterCallback(NetifCallback *callback);

  virtual ThreadError SendMessage(Message *message) = 0;
  virtual const char *GetName() const = 0;
  virtual ThreadError GetLinkAddress(LinkAddress *address) const = 0;

  static Netif *GetNetifList();
  static Netif *GetNetifById(uint8_t interface_id);
  static Netif *GetNetifByName(char *name);
  static bool IsAddress(const Ip6Address *address);
  static const NetifAddress *SelectSourceAddress(Ip6MessageInfo *message_info);
  static int GetOnLinkNetif(Ip6Address *address);

 private:
  static void HandleCallbackTask(void *context);
  void HandleCallbackTask();

  NetifCallback *callbacks_ = NULL;
  NetifAddress *unicast_addresses_ = NULL;
  NetifMulticastAddress *multicast_addresses_ = NULL;
  int interface_id_ = -1;
  bool all_routers_subscribed_ = false;
  Netif *next_ = NULL;
  Tasklet callback_task_;
};

}  // namespace Thread

#endif  // NET_NETIF_H_
