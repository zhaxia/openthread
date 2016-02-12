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

#include <coap/coap_message.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <mac/mac_frame.h>
#include <thread/address_resolver.h>
#include <thread/mesh_forwarder.h>
#include <thread/mle_router.h>
#include <thread/thread_tlvs.h>

using Thread::Encoding::BigEndian::HostSwap16;
using Thread::Encoding::BigEndian::HostSwap32;

namespace Thread {

AddressResolver::AddressResolver(MeshForwarder *mesh_forwarder, CoapServer *coap_server, MleRouter *mle,
                                 Netif *netif) :
    address_error_("a/ae", &HandleAddressError, this),
    address_query_("a/aq", &HandleAddressQuery, this),
    address_notification_("a/an", &HandleAddressNotification, this),
    icmp6_callbacks_(&HandleDstUnreach, this),
    socket_(&RecvFrom, this),
    timer_(&HandleTimer, this) {
  memset(&cache_, 0, sizeof(cache_));
  mesh_forwarder_ = mesh_forwarder;
  coap_server_ = coap_server;
  mle_ = mle;
  netif_ = netif;
  coap_message_id_ = Random::Get();

  coap_server->AddResource(&address_error_);
  coap_server->AddResource(&address_query_);
  coap_server->AddResource(&address_notification_);
  Icmp6::RegisterCallbacks(&icmp6_callbacks_);
}

ThreadError AddressResolver::Clear() {
  memset(&cache_, 0, sizeof(cache_));
  return kThreadError_None;
}

ThreadError AddressResolver::Remove(uint8_t router_id) {
  for (int i = 0; i < kCacheEntries; i++) {
    if ((cache_[i].rloc >> 10) == router_id)
      cache_[i].state = Cache::kStateInvalid;
  }
  return kThreadError_None;
}

ThreadError AddressResolver::Resolve(const Ip6Address *dst, MacAddr16 *rloc) {
  ThreadError error = kThreadError_None;
  Cache *entry = NULL;

  for (int i = 0; i < kCacheEntries; i++) {
    if (cache_[i].state != Cache::kStateInvalid) {
      if (memcmp(&cache_[i].target, dst, sizeof(cache_[i].target)) == 0) {
        entry = &cache_[i];
        break;
      }
    } else if (entry == NULL) {
      entry = &cache_[i];
    }
  }

  VerifyOrExit(entry != NULL, error = kThreadError_NoBufs);

  switch (entry->state) {
    case Cache::kStateInvalid:
      memcpy(&entry->target, dst, sizeof(entry->target));
      entry->state = Cache::kStateDiscover;
      entry->timeout = kDiscoverTimeout;
      timer_.Start(1000);
      SendAddressQuery(dst);
      error = kThreadError_LeaseQuery;
      break;
    case Cache::kStateDiscover:
    case Cache::kStateRetry:
      error = kThreadError_LeaseQuery;
      break;
    case Cache::kStateValid:
      *rloc = entry->rloc;
      break;
  }

exit:
  return error;
}

ThreadError AddressResolver::SendAddressQuery(const Ip6Address *eid) {
  ThreadError error;

  struct sockaddr_in6 sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin6_port = kCoapUdpPort;
  socket_.Bind(&sockaddr);

  for (int i = 0; i < sizeof(coap_token_); i++)
    coap_token_[i] = Random::Get();

  CoapMessage coap;
  coap.SetVersion(1);
  coap.SetType(CoapMessage::kTypeNonConfirmable);
  coap.SetCode(CoapMessage::kCodePost);
  coap.SetMessageId(++coap_message_id_);
  coap.SetToken(NULL, 0);
  coap.AppendUriPathOptions("a/aq");
  coap.AppendContentFormatOption(CoapMessage::kApplicationOctetStream);
  coap.Finalize();

  Message *message;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = message->Append(coap.GetHeaderBytes(), coap.GetHeaderLength()));

  ThreadTargetEidTlv target_eid_tlv;
  target_eid_tlv.header.type = ThreadTlv::kTypeTargetEid;
  target_eid_tlv.header.length = sizeof(target_eid_tlv) - sizeof(target_eid_tlv.header);
  memcpy(&target_eid_tlv.address, eid, sizeof(target_eid_tlv.address));
  SuccessOrExit(error = message->Append(&target_eid_tlv, sizeof(target_eid_tlv)));

  Ip6MessageInfo message_info;
  memset(&message_info, 0, sizeof(message_info));
  message_info.peer_addr.s6_addr16[0] = HostSwap16(0xff03);
  message_info.peer_addr.s6_addr16[7] = HostSwap16(0x0002);
  message_info.peer_port = kCoapUdpPort;
  message_info.interface_id = netif_->GetInterfaceId();

  SuccessOrExit(error = socket_.SendTo(message, &message_info));

  dprintf("Sent address query\n");

exit:
  if (error != kThreadError_None && message != NULL)
    Message::Free(message);
  return error;
}

ThreadError AddressResolver::FindTlv(Message *message, uint8_t type, void *buf, uint16_t buf_length) {
  ThreadError error = kThreadError_Parse;
  uint16_t offset = message->GetOffset();
  uint16_t end = message->GetLength();

  while (offset < end) {
    ThreadTlv tlv;
    message->Read(offset, sizeof(tlv), &tlv);
    if (tlv.type == type && (offset + sizeof(tlv) + tlv.length) <= end) {
      if (buf_length > sizeof(tlv) + tlv.length)
        buf_length = sizeof(tlv) + tlv.length;
      message->Read(offset, buf_length, buf);

      ExitNow(error = kThreadError_None);
    }
    offset += sizeof(tlv) + tlv.length;
  }

exit:
  return error;
}

void AddressResolver::RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info) {
}

void AddressResolver::HandleAddressNotification(void *context, CoapMessage *coap_message, Message *message,
                                                const Ip6MessageInfo *message_info) {
  AddressResolver *obj = reinterpret_cast<AddressResolver*>(context);
  obj->HandleAddressNotification(coap_message, message, message_info);
}

void AddressResolver::HandleAddressNotification(CoapMessage *coap_message, Message *message,
                                                const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Message *reply = NULL;

  VerifyOrExit(coap_message->GetType() == CoapMessage::kTypeConfirmable &&
               coap_message->GetCode() == CoapMessage::kCodePost, ;);

  dprintf("Received address notification from %04x\n", HostSwap16(message_info->peer_addr.s6_addr16[7]));

  ThreadTargetEidTlv target_eid_tlv;
  SuccessOrExit(FindTlv(message, ThreadTlv::kTypeTargetEid, &target_eid_tlv, sizeof(target_eid_tlv)));
  VerifyOrExit(target_eid_tlv.header.length == sizeof(target_eid_tlv) - sizeof(target_eid_tlv.header), ;);

  ThreadMeshLocalIidTlv ml_eid_tlv;
  SuccessOrExit(FindTlv(message, ThreadTlv::kTypeMeshLocalIid, &ml_eid_tlv, sizeof(ml_eid_tlv)));
  VerifyOrExit(ml_eid_tlv.header.length == sizeof(ml_eid_tlv) - sizeof(ml_eid_tlv.header), ;);

  ThreadRlocTlv rloc_tlv;
  SuccessOrExit(FindTlv(message, ThreadTlv::kTypeRloc, &rloc_tlv, sizeof(rloc_tlv)));
  VerifyOrExit(rloc_tlv.header.length == sizeof(rloc_tlv) - sizeof(rloc_tlv.header), ;);

  for (int i = 0; i < kCacheEntries; i++) {
    if (memcmp(&cache_[i].target, &target_eid_tlv.address, sizeof(cache_[i].target)) == 0) {
      if (cache_[i].state != Cache::kStateValid ||
          memcmp(cache_[i].iid, &ml_eid_tlv.iid, sizeof(cache_[i].iid)) == 0) {
        memcpy(cache_[i].iid, &ml_eid_tlv.iid, sizeof(cache_[i].iid));
        cache_[i].rloc = HostSwap16(rloc_tlv.address);
        cache_[i].timeout = 0;
        cache_[i].failure_count = 0;
        cache_[i].state = Cache::kStateValid;
        goto found;
      } else {
        SendAddressError(&target_eid_tlv, &ml_eid_tlv, NULL);
        ExitNow();
      }
    }
  }

  ExitNow();

found:
  uint16_t message_id;
  message_id = coap_message->GetMessageId();

  uint8_t token_length;
  token_length = coap_message->GetTokenLength();

  uint8_t token[CoapMessage::kMaxTokenLength];
  memcpy(token, coap_message->GetToken(NULL), coap_message->GetTokenLength());

  coap_message->Init();
  coap_message->SetVersion(1);
  coap_message->SetType(CoapMessage::kTypeAcknowledgment);
  coap_message->SetCode(CoapMessage::kCodeChanged);
  coap_message->SetMessageId(message_id);
  coap_message->SetToken(token, token_length);
  coap_message->Finalize();

  VerifyOrExit((reply = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = reply->Append(coap_message->GetHeaderBytes(), coap_message->GetHeaderLength()));

  Ip6MessageInfo reply_info;
  memcpy(&reply_info, message_info, sizeof(reply_info));
  memset(&reply_info.sock_addr, 0, sizeof(reply_info.sock_addr));
  SuccessOrExit(error = coap_server_->SendMessage(reply, &reply_info));

  dprintf("Sent address notification acknowledgment\n");

  mesh_forwarder_->HandleResolved(&target_eid_tlv.address);

exit:
  if (error != kThreadError_None && reply != NULL)
    Message::Free(reply);
}

ThreadError AddressResolver::SendAddressError(const ThreadTargetEidTlv *target, const ThreadMeshLocalIidTlv *eid,
                                              const Ip6Address *destination) {
  ThreadError error;

  struct sockaddr_in6 sockaddr;
  memset(&sockaddr, 0, sizeof(sockaddr));
  sockaddr.sin6_port = kCoapUdpPort;
  socket_.Bind(&sockaddr);

  for (int i = 0; i < sizeof(coap_token_); i++)
    coap_token_[i] = Random::Get();

  CoapMessage coap;
  coap.SetVersion(1);
  coap.SetType(CoapMessage::kTypeNonConfirmable);
  coap.SetCode(CoapMessage::kCodePost);
  coap.SetMessageId(++coap_message_id_);
  coap.SetToken(NULL, 0);
  coap.AppendUriPathOptions("a/ae");
  coap.AppendContentFormatOption(CoapMessage::kApplicationOctetStream);
  coap.Finalize();

  Message *message;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = message->Append(coap.GetHeaderBytes(), coap.GetHeaderLength()));
  SuccessOrExit(error = message->Append(target, sizeof(*target)));
  SuccessOrExit(error = message->Append(eid, sizeof(*eid)));

  Ip6MessageInfo message_info;
  memset(&message_info, 0, sizeof(message_info));
  if (destination == NULL) {
    message_info.peer_addr.s6_addr16[0] = HostSwap16(0xff03);
    message_info.peer_addr.s6_addr16[7] = HostSwap16(0x0002);
  } else {
    memcpy(&message_info.peer_addr, destination, sizeof(message_info.peer_addr));
  }
  message_info.peer_port = kCoapUdpPort;
  message_info.interface_id = netif_->GetInterfaceId();

  SuccessOrExit(error = socket_.SendTo(message, &message_info));

  dprintf("Sent address error\n");

exit:
  if (error != kThreadError_None && message != NULL)
    Message::Free(message);
  return error;
}

void AddressResolver::HandleAddressError(void *context, CoapMessage *coap_message,
                                         Message *message, const Ip6MessageInfo *message_info) {
  AddressResolver *obj = reinterpret_cast<AddressResolver*>(context);
  obj->HandleAddressError(coap_message, message, message_info);
}

void AddressResolver::HandleAddressError(CoapMessage *coap_message, Message *message,
                                         const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(coap_message->GetCode() == CoapMessage::kCodePost, error = kThreadError_Drop);

  dprintf("Received address error notification\n");

  ThreadTargetEidTlv target_eid_tlv;
  SuccessOrExit(error = FindTlv(message, ThreadTlv::kTypeTargetEid, &target_eid_tlv, sizeof(target_eid_tlv)));
  VerifyOrExit(target_eid_tlv.header.length == sizeof(target_eid_tlv) - sizeof(target_eid_tlv.header),
               error = kThreadError_Parse);

  ThreadMeshLocalIidTlv ml_eid_tlv;
  SuccessOrExit(error = FindTlv(message, ThreadTlv::kTypeMeshLocalIid, &ml_eid_tlv, sizeof(ml_eid_tlv)));
  VerifyOrExit(ml_eid_tlv.header.length == sizeof(ml_eid_tlv) - sizeof(ml_eid_tlv.header),
               error = kThreadError_Parse);

  for (const NetifAddress *address = netif_->GetAddresses(); address; address = address->next_) {
    if (memcmp(&address->address, &target_eid_tlv.address, sizeof(address->address)) == 0 &&
        memcmp(ml_eid_tlv.iid, mle_->GetMeshLocal64()->s6_addr + 8, sizeof(ml_eid_tlv.iid))) {
      // Target EID matches address and Mesh Local EID differs
      netif_->RemoveAddress(address);
      ExitNow();
    }
  }

  Child *children;
  uint8_t num_children;
  children = mle_->GetChildren(&num_children);

  MacAddr64 mac_addr;
  memcpy(&mac_addr, ml_eid_tlv.iid, sizeof(mac_addr));
  mac_addr.bytes[0] ^= 0x2;

  for (int i = 0; i < Mle::kMaxChildren; i++) {
    if (children[i].state != Neighbor::kStateValid || (children[i].mode & Mle::kModeFFD) != 0)
      continue;

    for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++) {
      if (memcmp(&children[i].ip6_address[j], &target_eid_tlv.address, sizeof(children[i].ip6_address[j])) == 0 &&
          memcmp(&children[i].mac_addr, &mac_addr, sizeof(children[i].mac_addr))) {
        // Target EID matches child address and Mesh Local EID differs on child
        memset(&children[i].ip6_address[j], 0, sizeof(&children[i].ip6_address[j]));

        Ip6Address destination;
        memset(&destination, 0, sizeof(destination));
        destination.s6_addr16[0] = HostSwap16(0xfe80);
        memcpy(destination.s6_addr + 8, &children[i].mac_addr, sizeof(ml_eid_tlv.iid));
        destination.s6_addr[8] ^= 0x2;

        SendAddressError(&target_eid_tlv, &ml_eid_tlv, &destination);
        ExitNow();
      }
    }
  }

exit:
  {}
}

void AddressResolver::HandleAddressQuery(void *context, CoapMessage *coap_message, Message *message,
                                         const Ip6MessageInfo *message_info) {
  AddressResolver *obj = reinterpret_cast<AddressResolver*>(context);
  obj->HandleAddressQuery(coap_message, message, message_info);
}

void AddressResolver::HandleAddressQuery(CoapMessage *coap_message, Message *message,
                                         const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Message *reply = NULL;

  VerifyOrExit(coap_message->GetType() == CoapMessage::kTypeNonConfirmable &&
               coap_message->GetCode() == CoapMessage::kCodePost, ;);

  dprintf("Received address query from %04x\n", HostSwap16(message_info->peer_addr.s6_addr16[7]));

  ThreadTargetEidTlv target_eid_tlv;
  SuccessOrExit(error = FindTlv(message, ThreadTlv::kTypeTargetEid, &target_eid_tlv, sizeof(target_eid_tlv)));
  VerifyOrExit(target_eid_tlv.header.length == sizeof(target_eid_tlv) - sizeof(target_eid_tlv.header),
               error = kThreadError_Parse);

  ThreadMeshLocalIidTlv ml_eid_tlv;
  ml_eid_tlv.header.type = ThreadTlv::kTypeMeshLocalIid;
  ml_eid_tlv.header.length = sizeof(ml_eid_tlv) - sizeof(ml_eid_tlv.header);

  ThreadLastTransactionTimeTlv last_transaction_time_tlv;
  last_transaction_time_tlv.header.type = ThreadTlv::kTypeLastTransactionTime;
  last_transaction_time_tlv.header.length =
      sizeof(last_transaction_time_tlv) - sizeof(last_transaction_time_tlv.header);

  bool child_eid;
  child_eid = false;

  if (netif_->IsAddress(&target_eid_tlv.address)) {
    memcpy(ml_eid_tlv.iid, mle_->GetMeshLocal64()->s6_addr + 8, sizeof(ml_eid_tlv.iid));
    goto found;
  }

  Child *children;
  uint8_t num_children;
  children = mle_->GetChildren(&num_children);

  for (int i = 0; i < Mle::kMaxChildren; i++) {
    if (children[i].state != Neighbor::kStateValid || (children[i].mode & Mle::kModeFFD) != 0)
      continue;

    for (int j = 0; j < Child::kMaxIp6AddressPerChild; j++) {
      if (memcmp(&children[i].ip6_address[j], &target_eid_tlv.address, sizeof(children[i].ip6_address[j])))
        continue;
      memcpy(ml_eid_tlv.iid, &children[i].mac_addr, sizeof(ml_eid_tlv.iid));
      ml_eid_tlv.iid[0] ^= 0x2;
      last_transaction_time_tlv.time = HostSwap32(Timer::GetNow() - children[i].last_heard);
      child_eid = true;
      goto found;
    }
  }

  ExitNow();

found:
  coap_message->Init();
  coap_message->SetVersion(1);
  coap_message->SetType(CoapMessage::kTypeConfirmable);
  coap_message->SetCode(CoapMessage::kCodePost);
  coap_message->SetMessageId(++coap_message_id_);
  coap_message->SetToken(NULL, 0);
  coap_message->AppendUriPathOptions("a/an");
  coap_message->AppendContentFormatOption(CoapMessage::kApplicationOctetStream);
  coap_message->Finalize();

  VerifyOrExit((reply = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = reply->Append(coap_message->GetHeaderBytes(), coap_message->GetHeaderLength()));
  SuccessOrExit(error = reply->Append(&target_eid_tlv, sizeof(target_eid_tlv)));
  SuccessOrExit(error = reply->Append(&ml_eid_tlv, sizeof(ml_eid_tlv)));

  ThreadRlocTlv rloc_tlv;
  rloc_tlv.header.type = ThreadTlv::kTypeRloc;
  rloc_tlv.header.length = sizeof(rloc_tlv) - sizeof(rloc_tlv.header);
  rloc_tlv.address = HostSwap16(mle_->GetAddress16());
  SuccessOrExit(error = reply->Append(&rloc_tlv, sizeof(rloc_tlv)));

  if (child_eid)
    SuccessOrExit(error = reply->Append(&last_transaction_time_tlv, sizeof(last_transaction_time_tlv)));

  Ip6MessageInfo reply_info;
  memset(&reply_info, 0, sizeof(reply_info));
  memcpy(&reply_info.peer_addr, &message_info->peer_addr, sizeof(reply_info.peer_addr));
  reply_info.interface_id = message_info->interface_id;
  reply_info.peer_port = kCoapUdpPort;

  SuccessOrExit(error = socket_.SendTo(reply, &reply_info));

  dprintf("Sent address notification\n");

exit:
  if (error != kThreadError_None && reply != NULL)
    Message::Free(reply);
}

void AddressResolver::HandleTimer(void *context) {
  AddressResolver *obj = reinterpret_cast<AddressResolver*>(context);
  obj->HandleTimer();
}

void AddressResolver::HandleTimer() {
  bool continue_timer = false;

  for (int i = 0; i < kCacheEntries; i++) {
    switch (cache_[i].state) {
      case Cache::kStateDiscover:
        cache_[i].timeout--;
        if (cache_[i].timeout == 0)
          cache_[i].state = Cache::kStateInvalid;
        else
          continue_timer = true;
        break;
      default:
        break;
    }
  }

  if (continue_timer)
    timer_.Start(1000);
}

const AddressResolver::Cache *AddressResolver::GetCacheEntries(uint16_t *num_entries) const {
  if (num_entries)
    *num_entries = kCacheEntries;
  return cache_;
}

void AddressResolver::HandleDstUnreach(void *context, Message *message, const Icmp6Header *icmp6_header,
                                       const Ip6MessageInfo *message_info) {
  AddressResolver *obj = reinterpret_cast<AddressResolver*>(context);
  obj->HandleDstUnreach(message, icmp6_header, message_info);
}

void AddressResolver::HandleDstUnreach(Message *message, const Icmp6Header *icmp6_header,
                                       const Ip6MessageInfo *message_info) {
  VerifyOrExit(icmp6_header->icmp6_code == ICMP6_DST_UNREACH_NOROUTE, ;);

  Ip6Header ip6_header;
  VerifyOrExit(message->Read(message->GetOffset(), sizeof(ip6_header), &ip6_header) == sizeof(ip6_header), ;);

  for (int i = 0; i < kCacheEntries; i++) {
    if (cache_[i].state != Cache::kStateInvalid &&
        memcmp(&cache_[i].target, &ip6_header.ip6_dst, sizeof(cache_[i].target)) == 0) {
      cache_[i].state = Cache::kStateInvalid;
      dprintf("cache entry removed!\n");
      break;
    }
  }

exit:
  {}
}

}  // namespace Thread
