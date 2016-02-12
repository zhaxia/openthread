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

#include <thread/network_data_leader.h>
#include <coap/coap_message.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/message.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <common/timer.h>
#include <mac/mac_frame.h>
#include <thread/mle.h>
#include <thread/thread_tlvs.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

NetworkDataLeader::NetworkDataLeader(CoapServer *coap_server, Netif *netif, Mle *mle):
    server_data_("n/sd", &HandleServerData, this),
    timer_(&HandleTimer, this) {
  coap_server_ = coap_server;
  netif_ = netif;
  mle_ = mle;
  memset(addresses_, 0, sizeof(addresses_));
  memset(context_last_used_, 0, sizeof(context_last_used_));
}

ThreadError NetworkDataLeader::Init() {
  version_ = Random::Get();
  stable_version_ = Random::Get();
  length_ = 0;
  return kThreadError_None;
}

ThreadError NetworkDataLeader::Start() {
  return coap_server_->AddResource(&server_data_);
}

ThreadError NetworkDataLeader::Stop() {
  return kThreadError_None;
}

uint8_t NetworkDataLeader::GetVersion() const {
  return version_;
}

ThreadError NetworkDataLeader::SetVersion(uint8_t version) {
  version_ = version;
  return kThreadError_None;
}

uint8_t NetworkDataLeader::GetStableVersion() const {
  return stable_version_;
}

ThreadError NetworkDataLeader::SetStableVersion(uint8_t stable_version) {
  stable_version_ = stable_version;
  return kThreadError_None;
}

uint32_t NetworkDataLeader::GetContextIdReuseDelay() const {
  return context_id_reuse_delay_;
}

ThreadError NetworkDataLeader::SetContextIdReuseDelay(uint32_t delay) {
  context_id_reuse_delay_ = delay;
  return kThreadError_None;
}

ThreadError NetworkDataLeader::GetContext(const Ip6Address &address, Context *context) {
  context->prefix_length = 0;

  if (PrefixMatch(mle_->GetMeshLocalPrefix(), address.s6_addr, 64)) {
    context->prefix = mle_->GetMeshLocalPrefix();
    context->prefix_length = 64;
    context->context_id = 0;
  }

  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);
       cur < reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
       cur = cur->GetNext()) {
    if (cur->GetType() != NetworkDataTlv::kTypePrefix)
      continue;
    PrefixTlv *prefix = reinterpret_cast<PrefixTlv*>(cur);
    if (!PrefixMatch(prefix->GetPrefix(), address.s6_addr, prefix->GetPrefixLength()))
      continue;
    ContextTlv *context_tlv = FindContext(prefix);
    if (context_tlv == NULL)
      continue;
    if (prefix->GetPrefixLength() > context->prefix_length) {
      context->prefix = prefix->GetPrefix();
      context->prefix_length = prefix->GetPrefixLength();
      context->context_id = context_tlv->GetContextId();
    }
  }

  return (context->prefix_length > 0) ? kThreadError_None : kThreadError_Error;
}

ThreadError NetworkDataLeader::GetContext(uint8_t context_id, Context *context) {
  ThreadError error = kThreadError_Error;

  if (context_id == 0) {
    context->prefix = mle_->GetMeshLocalPrefix();
    context->prefix_length = 64;
    context->context_id = 0;
    ExitNow(error = kThreadError_None);
  }

  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);
       cur < reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
       cur = cur->GetNext()) {
    if (cur->GetType() != NetworkDataTlv::kTypePrefix)
      continue;
    PrefixTlv *prefix = reinterpret_cast<PrefixTlv*>(cur);
    ContextTlv *context_tlv = FindContext(prefix);
    if (context_tlv == NULL)
      continue;
    if (context_tlv->GetContextId() != context_id)
      continue;
    context->prefix = prefix->GetPrefix();
    context->prefix_length = prefix->GetPrefixLength();
    context->context_id = context_tlv->GetContextId();
    ExitNow(error = kThreadError_None);
  }

exit:
  return error;
}

ThreadError NetworkDataLeader::ConfigureAddresses() {
  // clear out addresses that are not on-mesh
  for (int i = 0; i < sizeof(addresses_)/sizeof(addresses_[0]); i++) {
    if (addresses_[i].valid_lifetime == 0 ||
        IsOnMesh(addresses_[i].address))
      continue;
    netif_->RemoveAddress(&addresses_[i]);
    addresses_[i].valid_lifetime = 0;
  }

  // configure on-mesh addresses
  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);
       cur < reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
       cur = cur->GetNext()) {
    if (cur->GetType() != NetworkDataTlv::kTypePrefix)
      continue;
    PrefixTlv *prefix = reinterpret_cast<PrefixTlv*>(cur);
    ConfigureAddress(prefix);
  }

  return kThreadError_None;
}

ThreadError NetworkDataLeader::ConfigureAddress(PrefixTlv *prefix) {
  // look for Border Router TLV
  BorderRouterTlv *border_router;
  if ((border_router = FindBorderRouter(prefix)) == NULL)
    ExitNow();

  // check if Valid flag is set
  BorderRouterEntry *entry;
  if ((entry = border_router->GetEntry(0)) == NULL ||
      entry->IsValid() == false)
    ExitNow();

  // check if address is already added for this prefix
  for (int i = 0; i < sizeof(addresses_)/sizeof(addresses_[0]); i++) {
    if (addresses_[i].valid_lifetime != 0 &&
        addresses_[i].prefix_length == prefix->GetPrefixLength() &&
        PrefixMatch(addresses_[i].address.s6_addr, prefix->GetPrefix(), prefix->GetPrefixLength())) {
      addresses_[i].preferred_lifetime = entry->IsPreferred() ? 0xffffffff : 0;
      ExitNow();
    }
  }

  // configure address for this prefix
  for (int i = 0; i < sizeof(addresses_)/sizeof(addresses_[0]); i++) {
    if (addresses_[i].valid_lifetime != 0)
      continue;
    memset(&addresses_[i], 0, sizeof(addresses_[i]));
    memcpy(addresses_[i].address.s6_addr, prefix->GetPrefix(), (prefix->GetPrefixLength()+7)/8);
    for (int j = 8; j < sizeof(addresses_[i].address); j++)
      addresses_[i].address.s6_addr[j] = Random::Get();
    addresses_[i].prefix_length = prefix->GetPrefixLength();
    addresses_[i].preferred_lifetime = entry->IsPreferred() ? 0xffffffff : 0;
    addresses_[i].valid_lifetime = 0xffffffff;
    netif_->AddAddress(&addresses_[i]);
    break;
  }

exit:
  return kThreadError_None;
}

ContextTlv *NetworkDataLeader::FindContext(PrefixTlv *prefix) {
  NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs());
  NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs() + prefix->GetSubTlvsLength());

  while (cur < end) {
    if (cur->GetType() == NetworkDataTlv::kTypeContext)
      return reinterpret_cast<ContextTlv*>(cur);
    cur = cur->GetNext();
  }

  return NULL;
}

bool NetworkDataLeader::IsOnMesh(const Ip6Address &address) {
  bool rval = false;

  if (memcmp(address.s6_addr, mle_->GetMeshLocalPrefix(), 8) == 0)
    ExitNow(rval = true);

  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);
       cur < reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
       cur = cur->GetNext()) {
    if (cur->GetType() != NetworkDataTlv::kTypePrefix)
      continue;
    PrefixTlv *prefix = reinterpret_cast<PrefixTlv*>(cur);
    if (PrefixMatch(prefix->GetPrefix(), address.s6_addr, prefix->GetPrefixLength()) == false)
      continue;
    if (FindBorderRouter(prefix) == NULL)
      continue;
    ExitNow(rval = true);
  }

exit:
  return rval;
}

ThreadError NetworkDataLeader::RouteLookup(const Ip6Address &source, const Ip6Address &destination, uint16_t *rloc) {
  ThreadError error = kThreadError_Error;

  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);
       cur < reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
       cur = cur->GetNext()) {
    if (cur->GetType() != NetworkDataTlv::kTypePrefix)
      continue;

    PrefixTlv *prefix = reinterpret_cast<PrefixTlv*>(cur);
    if (PrefixMatch(prefix->GetPrefix(), source.s6_addr, prefix->GetPrefixLength())) {
      // search for external routes first

      // select border router
      if (RouteLookup(prefix, rloc) == kThreadError_None)
        ExitNow(error = kThreadError_None);
    }
  }

exit:
  return error;
}

ThreadError NetworkDataLeader::RouteLookup(PrefixTlv *prefix, uint16_t *rloc) {
  ThreadError error = kThreadError_Error;

  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs());
       cur < reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs() + prefix->GetSubTlvsLength());
       cur = cur->GetNext()) {
    if (cur->GetType() != NetworkDataTlv::kTypeBorderRouter)
      continue;
    BorderRouterTlv *border_router = reinterpret_cast<BorderRouterTlv*>(cur);
    for (int i = 0; i < border_router->GetNumEntries(); i++) {
      BorderRouterEntry *entry = border_router->GetEntry(i);
      if (entry->IsDefaultRoute() == false)
        continue;
      *rloc = entry->GetRloc();
      ExitNow(error = kThreadError_None);
    }
  }

exit:
  return error;
}

ThreadError NetworkDataLeader::SetNetworkData(uint8_t version, uint8_t stable_version, bool stable,
                                              const uint8_t *data, uint8_t data_length) {
  version_ = version;
  stable_version_ = stable_version;
  memcpy(tlvs_, data, data_length);
  length_ = data_length;

  if (stable)
    RemoveTemporaryData(tlvs_, &length_);

  dump("set network data", tlvs_, length_);

  ConfigureAddresses();
  mle_->HandleNetworkDataUpdate();

  return kThreadError_None;
}

ThreadError NetworkDataLeader::RemoveBorderRouter(uint16_t rloc) {
  RemoveRloc(rloc);
  ConfigureAddresses();
  mle_->HandleNetworkDataUpdate();
  return kThreadError_None;
}

void NetworkDataLeader::HandleServerData(void *context, CoapMessage *coap_message, Message *message,
                                         const Ip6MessageInfo *message_info) {
  NetworkDataLeader *obj = reinterpret_cast<NetworkDataLeader*>(context);
  obj->HandleServerData(coap_message, message, message_info);
}

void NetworkDataLeader::HandleServerData(CoapMessage *coap_message, Message *message,
                                         const Ip6MessageInfo *message_info) {
  ThreadError error = kThreadError_None;

  dprintf("Received network data registration\n");

  uint8_t tlvs_length;
  tlvs_length = message->GetLength() - message->GetOffset();

  uint8_t tlvs[256];
  message->Read(message->GetOffset(), tlvs_length, tlvs);

  uint16_t address16;
  address16 = HostSwap16(message_info->peer_addr.s6_addr16[7]);

  RegisterNetworkData(address16, tlvs, tlvs_length);

  uint16_t message_id;
  message_id = coap_message->GetMessageId();

  uint8_t token_length;
  token_length = coap_message->GetTokenLength();

  uint8_t token[CoapMessage::kMaxTokenLength];
  memcpy(token, coap_message->GetToken(NULL), token_length);

  coap_message->Init();
  coap_message->SetVersion(1);
  coap_message->SetType(CoapMessage::kTypeAcknowledgment);
  coap_message->SetCode(CoapMessage::kCodeChanged);
  coap_message->SetMessageId(message_id);
  coap_message->SetToken(token, token_length);
  coap_message->Finalize();

  Message *reply;
  VerifyOrExit((reply = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = reply->Append(coap_message->GetHeaderBytes(), coap_message->GetHeaderLength()));
  SuccessOrExit(error = reply->Append(tlvs, tlvs_length));

  dump("help", message_info, sizeof(*message_info));

  SuccessOrExit(error = coap_server_->SendMessage(reply, message_info));

  dprintf("Sent network data registration acknowledgment\n");

exit:
  if (error != kThreadError_None && reply != NULL)
    Message::Free(reply);
}

ThreadError NetworkDataLeader::RegisterNetworkData(uint16_t rloc, uint8_t *tlvs, uint8_t tlvs_length) {
  ThreadError error = kThreadError_None;

  SuccessOrExit(error = RemoveRloc(rloc));
  SuccessOrExit(error = AddNetworkData(tlvs, tlvs_length));

  version_++;
  stable_version_++;

  ConfigureAddresses();
  mle_->HandleNetworkDataUpdate();

exit:
  return error;
}

ThreadError NetworkDataLeader::AddNetworkData(uint8_t *tlvs, uint8_t tlvs_length) {
  NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs);
  NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv*>(tlvs + tlvs_length);

  while (cur < end) {
    switch (cur->GetType()) {
      case NetworkDataTlv::kTypePrefix:
        AddPrefix(reinterpret_cast<PrefixTlv*>(cur));
        dump("add prefix done", tlvs_, length_);
        break;
      default:
        assert(false);
        break;
    }
    cur = cur->GetNext();
  }

  dump("add done", tlvs_, length_);

  return kThreadError_None;
}

ThreadError NetworkDataLeader::AddPrefix(PrefixTlv *prefix) {
  NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs());
  NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs() + prefix->GetSubTlvsLength());

  while (cur < end) {
    switch (cur->GetType()) {
      case NetworkDataTlv::kTypeBorderRouter:
        AddBorderRouter(prefix, reinterpret_cast<BorderRouterTlv*>(cur));
        break;
      default:
        assert(false);
        break;
    }
    cur = cur->GetNext();
  }

  return kThreadError_None;
}

ThreadError NetworkDataLeader::AddBorderRouter(PrefixTlv *prefix, BorderRouterTlv *border_router) {
  ThreadError error = kThreadError_None;
  PrefixTlv *dst_prefix;
  if ((dst_prefix = FindPrefix(prefix->GetPrefix(), prefix->GetPrefixLength())) == NULL) {
    dst_prefix = reinterpret_cast<PrefixTlv*>(tlvs_ + length_);
    Insert(reinterpret_cast<uint8_t*>(dst_prefix), sizeof(PrefixTlv) + (prefix->GetPrefixLength()+7)/8);
    dst_prefix->Init(prefix->GetDomainId(), prefix->GetPrefixLength(), prefix->GetPrefix());
  }

  if (border_router->IsStable()) {
    dst_prefix->SetStable();

    ContextTlv *dst_context;
    uint8_t context_id;
    if ((dst_context = FindContext(dst_prefix)) != NULL) {
      dst_context->SetCompress();
    } else if ((context_id = AllocateContext()) >= 0) {
      dst_context = reinterpret_cast<ContextTlv*>(dst_prefix->GetNext());
      Insert(reinterpret_cast<uint8_t*>(dst_context), sizeof(ContextTlv));
      dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(ContextTlv));
      dst_context->Init();
      dst_context->SetStable();
      dst_context->SetCompress();
      dst_context->SetContextId(context_id);
      dst_context->SetContextLength(prefix->GetPrefixLength());
    } else {
      ExitNow(error = kThreadError_NoBufs);
    }
    context_last_used_[dst_context->GetContextId() - kMinContextId] = 0;
  }

  BorderRouterTlv *dst_border_router;
  if ((dst_border_router = FindBorderRouter(dst_prefix, border_router->IsStable())) == NULL) {
    dst_border_router = reinterpret_cast<BorderRouterTlv*>(dst_prefix->GetNext());
    Insert(reinterpret_cast<uint8_t*>(dst_border_router), sizeof(BorderRouterTlv));
    dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(BorderRouterTlv));
    dst_border_router->Init();
    if (border_router->IsStable())
      dst_border_router->SetStable();
  }

  Insert(reinterpret_cast<uint8_t*>(dst_border_router->GetNext()), sizeof(BorderRouterEntry));
  dst_border_router->SetLength(dst_border_router->GetLength() + sizeof(BorderRouterEntry));
  dst_prefix->SetLength(dst_prefix->GetLength() + sizeof(BorderRouterEntry));
  memcpy(dst_border_router->GetEntry(dst_border_router->GetNumEntries()-1), border_router->GetEntry(0),
         sizeof(BorderRouterEntry));

exit:
  return error;
}

int NetworkDataLeader::AllocateContext() {
  int rval = -1;

  for (int i = kMinContextId; i < kMinContextId + kNumContextIds; i++) {
    if ((context_used_ & (1 << i)) == 0) {
      context_used_ |= 1 << i;
      rval = i;
      dprintf("Allocated Context ID = %d\n", rval);
      ExitNow();
    }
  }

exit:
  return rval;
}

ThreadError NetworkDataLeader::FreeContext(uint8_t context_id) {
  dprintf("Free Context Id = %d\n", context_id);
  RemoveContext(context_id);
  context_used_ &= ~(1 << context_id);
  version_++;
  stable_version_++;
  mle_->HandleNetworkDataUpdate();
  return kThreadError_None;
}

ThreadError NetworkDataLeader::RemoveRloc(uint16_t rloc) {
  NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);

  while (1) {
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
    if (cur >= end)
      break;

    switch (cur->GetType()) {
      case NetworkDataTlv::kTypePrefix: {
        PrefixTlv *prefix = reinterpret_cast<PrefixTlv*>(cur);
        RemoveRloc(prefix, rloc);
        if (prefix->GetSubTlvsLength() == 0) {
          Remove(reinterpret_cast<uint8_t*>(prefix), sizeof(NetworkDataTlv) + prefix->GetLength());
          continue;
        }
        dump("remove prefix done", tlvs_, length_);
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    cur = cur->GetNext();
  }

  dump("remove done", tlvs_, length_);

  return kThreadError_None;
}

ThreadError NetworkDataLeader::RemoveRloc(PrefixTlv *prefix, uint16_t rloc) {
  NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs());

  while (1) {
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs() + prefix->GetSubTlvsLength());
    if (cur >= end)
      break;

    switch (cur->GetType()) {
      case NetworkDataTlv::kTypeBorderRouter: {
        BorderRouterTlv *border_router = reinterpret_cast<BorderRouterTlv*>(cur);

        // remove rloc from border router tlv
        for (int i = 0; i < border_router->GetNumEntries(); i++) {
          BorderRouterEntry *entry = border_router->GetEntry(i);
          if (entry->GetRloc() != rloc)
            continue;
          border_router->SetLength(border_router->GetLength() - sizeof(BorderRouterEntry));
          prefix->SetSubTlvsLength(prefix->GetSubTlvsLength() - sizeof(BorderRouterEntry));
          Remove(reinterpret_cast<uint8_t*>(entry), sizeof(*entry));
          break;
        }

        // remove border router tlv if empty
        if (border_router->GetNumEntries() == 0) {
          uint8_t length = sizeof(BorderRouterTlv) + border_router->GetLength();
          prefix->SetSubTlvsLength(prefix->GetSubTlvsLength() - length);
          Remove(reinterpret_cast<uint8_t*>(border_router), length);
          continue;
        }
        break;
      }
      case NetworkDataTlv::kTypeContext: {
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    cur = cur->GetNext();
  }

  ContextTlv *context;
  if ((context = FindContext(prefix)) != NULL) {
    if (prefix->GetSubTlvsLength() == sizeof(ContextTlv)) {
      context->ClearCompress();
      context_last_used_[context->GetContextId() - kMinContextId] = Timer::GetNow();
      if (context_last_used_[context->GetContextId() - kMinContextId] == 0)
        context_last_used_[context->GetContextId() - kMinContextId] = 1;
      timer_.Start(1000);
    } else {
      context->SetCompress();
      context_last_used_[context->GetContextId() - kMinContextId] = 0;
    }
  }

  return kThreadError_None;
}

ThreadError NetworkDataLeader::RemoveContext(uint8_t context_id) {
  NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);

  while (1) {
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
    if (cur >= end)
      break;

    switch (cur->GetType()) {
      case NetworkDataTlv::kTypePrefix: {
        PrefixTlv *prefix = reinterpret_cast<PrefixTlv*>(cur);
        RemoveContext(prefix, context_id);
        if (prefix->GetSubTlvsLength() == 0) {
          Remove(reinterpret_cast<uint8_t*>(prefix), sizeof(NetworkDataTlv) + prefix->GetLength());
          continue;
        }
        dump("remove prefix done", tlvs_, length_);
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    cur = cur->GetNext();
  }

  dump("remove done", tlvs_, length_);

  return kThreadError_None;
}

ThreadError NetworkDataLeader::RemoveContext(PrefixTlv *prefix, uint8_t context_id) {
  NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs());

  while (1) {
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs() + prefix->GetSubTlvsLength());
    if (cur >= end)
      break;

    switch (cur->GetType()) {
      case NetworkDataTlv::kTypeBorderRouter: {
        break;
      }
      case NetworkDataTlv::kTypeContext: {
        // remove context tlv
        ContextTlv *context = reinterpret_cast<ContextTlv*>(cur);
        if (context->GetContextId() == context_id) {
          uint8_t length = sizeof(NetworkDataTlv) + context->GetLength();
          prefix->SetSubTlvsLength(prefix->GetSubTlvsLength() - length);
          Remove(reinterpret_cast<uint8_t*>(context), length);
          continue;
        }
        break;
      }
      default: {
        assert(false);
        break;
      }
    }
    cur = cur->GetNext();
  }

  return kThreadError_None;
}

void NetworkDataLeader::HandleTimer(void *context) {
  NetworkDataLeader *obj = reinterpret_cast<NetworkDataLeader*>(context);
  obj->HandleTimer();
}

void NetworkDataLeader::HandleTimer() {
  bool contexts_waiting = false;

  for (int i = 0; i < kNumContextIds; i++) {
    if (context_last_used_[i] == 0)
      continue;

    if ((Timer::GetNow() - context_last_used_[i]) >= context_id_reuse_delay_ * 1000U) {
      FreeContext(kMinContextId + i);
    } else {
      contexts_waiting = true;
    }
  }

  if (contexts_waiting)
    timer_.Start(1000);
}

}  // namespace Thread
