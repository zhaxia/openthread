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

#include <common/code_utils.h>
#include <common/debug.h>
#include <common/message.h>
#include <net/netif.h>

namespace Thread {

static Netif *netif_list_head_ = NULL;
static int next_interface_id_ = 1;

Netif::Netif() :
    callback_task_(&HandleCallbackTask, this) {
}

ThreadError Netif::RegisterCallback(NetifCallback *callback) {
  ThreadError error = kThreadError_None;

  for (NetifCallback *cur = callbacks_; cur; cur = cur->next_) {
    if (cur == callback)
      ExitNow(error = kThreadError_Busy);
  }

  callback->next_ = callbacks_;
  callbacks_ = callback;

exit:
  return error;
}

ThreadError Netif::AddNetif() {
  if (netif_list_head_ == NULL) {
    netif_list_head_ = this;
  } else {
    Netif *netif = netif_list_head_;
    do {
      if (netif == this)
        return kThreadError_Busy;
    } while (netif->next_);
    netif->next_ = this;
  }

  next_ = NULL;
  if (interface_id_ < 0)
    interface_id_ = next_interface_id_++;

  return kThreadError_None;
}

ThreadError Netif::RemoveNetif() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(netif_list_head_ != NULL, error = kThreadError_Busy);

  if (netif_list_head_ == this) {
    netif_list_head_ = next_;
  } else {
    for (Netif *cur = netif_list_head_; cur->next_; cur = cur->next_) {
      if (cur->next_ != this)
        continue;
      cur->next_ = next_;
      break;
    }
  }

  next_ = NULL;

exit:
  return error;
}

Netif *Netif::GetNext() const {
  return next_;
}

Netif *Netif::GetNetifById(uint8_t interface_id) {
  for (Netif *netif = netif_list_head_; netif; netif = netif->next_)
    if (netif->interface_id_ == interface_id)
      return netif;
  return NULL;
}

Netif *Netif::GetNetifByName(char *name) {
  for (Netif *netif = netif_list_head_; netif; netif = netif->next_) {
    if (strcmp(netif->GetName(), name) == 0)
      return netif;
  }
  return NULL;
}

int Netif::GetInterfaceId() const {
  return interface_id_;
}

bool Netif::IsMulticastSubscribed(Ip6Address *address) const {
  bool rval = false;

  if (address->IsLinkLocalAllNodesMulticast() || address->IsRealmLocalAllNodesMulticast())
    ExitNow(rval = true);
  else if (address->IsLinkLocalAllRoutersMulticast() || address->IsRealmLocalAllRoutersMulticast())
    ExitNow(rval = all_routers_subscribed_);

  for (NetifMulticastAddress *cur = multicast_addresses_; cur; cur = cur->next_) {
    dump("mcast", &cur->address_, sizeof(cur->address_));
    if (memcmp(&cur->address_, address, sizeof(cur->address_)) == 0) {
      ExitNow(rval = true);
    }
  }

exit:
  return rval;
}

ThreadError Netif::SubscribeAllRoutersMulticast() {
  all_routers_subscribed_ = true;
  return kThreadError_None;
}

ThreadError Netif::UnsubscribeAllRoutersMulticast() {
  all_routers_subscribed_ = false;
  return kThreadError_None;
}

ThreadError Netif::SubscribeMulticast(NetifMulticastAddress *address) {
  ThreadError error = kThreadError_None;

  for (NetifMulticastAddress *cur = multicast_addresses_; cur; cur = cur->next_) {
    if (cur == address)
      ExitNow(error = kThreadError_Busy);
  }
  address->next_ = multicast_addresses_;
  multicast_addresses_ = address;

exit:
  return error;
}

ThreadError Netif::UnsubscribeMulticast(NetifMulticastAddress *address) {
  ThreadError error = kThreadError_None;

  if (multicast_addresses_ == address) {
    multicast_addresses_ = multicast_addresses_->next_;
    ExitNow();
  } else if (multicast_addresses_ != NULL) {
    for (NetifMulticastAddress *cur = multicast_addresses_; cur->next_; cur = cur->next_) {
      if (cur->next_ == address) {
        cur->next_ = address->next_;
        ExitNow();
      }
    }
  }

  ExitNow(error = kThreadError_Error);

exit:
  address->next_ = NULL;
  return error;
}

const NetifAddress *Netif::GetAddresses() const {
  return unicast_addresses_;
}

ThreadError Netif::AddAddress(NetifAddress *address) {
  for (NetifAddress *cur = unicast_addresses_; cur; cur = cur->next_) {
    if (cur == address)
      return kThreadError_Error;
  }
  address->next_ = unicast_addresses_;
  unicast_addresses_ = address;

  callback_task_.Post();

  return kThreadError_None;
}

ThreadError Netif::RemoveAddress(const NetifAddress *address) {
  ThreadError error = kThreadError_None;

  if (unicast_addresses_ == address) {
    unicast_addresses_ = unicast_addresses_->next_;
    ExitNow();
  } else if (unicast_addresses_ != NULL) {
    for (NetifAddress *cur_address = unicast_addresses_; cur_address->next_; cur_address = cur_address->next_) {
      if (cur_address->next_ == address) {
        cur_address->next_ = address->next_;
        ExitNow();
      }
    }
  }

  ExitNow(error = kThreadError_Error);

exit:
  callback_task_.Post();
  return error;
}

Netif *Netif::GetNetifList() {
  return netif_list_head_;
}

bool Netif::IsAddress(const Ip6Address *address) {
  for (Netif *cur_netif = netif_list_head_; cur_netif; cur_netif = cur_netif->next_) {
    for (NetifAddress *cur_address = cur_netif->unicast_addresses_; cur_address; cur_address = cur_address->next_) {
      if (cur_address->address == *address)
        return true;
    }
  }
  return false;
}

const NetifAddress *Netif::SelectSourceAddress(Ip6MessageInfo *message_info) {
  Ip6Address *destination = &message_info->peer_addr;
  int interface_id = message_info->interface_id;

  const NetifAddress *rval_addr = NULL;
  uint8_t rval_iface = 0;

  for (Netif *netif = GetNetifList(); netif; netif = netif->next_) {
    uint8_t candidate_id = netif->GetInterfaceId();

    for (const NetifAddress *netif_addr = netif->GetAddresses(); netif_addr; netif_addr = netif_addr->next_) {
      const Ip6Address *candidate_addr = &netif_addr->address;

      if (destination->IsLinkLocal() || destination->IsMulticast()) {
        if (interface_id != candidate_id)
          continue;
      }

      if (rval_addr == NULL) {
        // Rule 0: Prefer any address
        rval_addr = netif_addr;
        rval_iface = candidate_id;
      } else if (*candidate_addr == *destination) {
        // Rule 1: Prefer same address
        rval_addr = netif_addr;
        rval_iface = candidate_id;
        goto exit;
      } else if (candidate_addr->GetScope() < rval_addr->address.GetScope()) {
        // Rule 2: Prefer appropriate scope
        if (candidate_addr->GetScope() >= destination->GetScope()) {
          rval_addr = netif_addr;
          rval_iface = candidate_id;
        }
      } else if (candidate_addr->GetScope() > rval_addr->address.GetScope()) {
        if (rval_addr->address.GetScope() < destination->GetScope()) {
          rval_addr = netif_addr;
          rval_iface = candidate_id;
        }
      } else if (netif_addr->preferred_lifetime != 0 && rval_addr->preferred_lifetime == 0) {
        // Rule 3: Avoid deprecated addresses
        rval_addr = netif_addr;
        rval_iface = candidate_id;
      } else if (message_info->interface_id != 0 && message_info->interface_id == candidate_id &&
                 rval_iface != candidate_id) {
        // Rule 4: Prefer home address
        // Rule 5: Prefer outgoing interface
        rval_addr = netif_addr;
        rval_iface = candidate_id;
      } else if (destination->PrefixMatch(candidate_addr) > destination->PrefixMatch(&rval_addr->address)) {
        // Rule 6: Prefer matching label
        // Rule 7: Prefer public address
        // Rule 8: Use longest prefix matching
        rval_addr = netif_addr;
        rval_iface = candidate_id;
      }
    }
  }

exit:
  message_info->interface_id = rval_iface;
  return rval_addr;
}

int Netif::GetOnLinkNetif(Ip6Address *address) {
  for (Netif *netif = netif_list_head_; netif; netif = netif->next_) {
    for (NetifAddress *netif_addr = netif->unicast_addresses_; netif_addr; netif_addr = netif_addr->next_) {
      if (netif_addr->address.PrefixMatch(address) >= netif_addr->prefix_length)
        return netif->interface_id_;
    }
  }
  return -1;
}

void Netif::HandleCallbackTask(void *context) {
  Netif *obj = reinterpret_cast<Netif*>(context);
  obj->HandleCallbackTask();
}

void Netif::HandleCallbackTask() {
  for (NetifCallback *cur = callbacks_; cur; cur = cur->next_)
    cur->callback_(cur->context_);
}

}  // namespace Thread
