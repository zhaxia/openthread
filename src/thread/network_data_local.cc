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

#include <thread/network_data_local.h>
#include <coap/coap_message.h>
#include <common/code_utils.h>
#include <common/random.h>
#include <thread/mle.h>
#include <thread/thread_tlvs.h>

namespace Thread {

NetworkDataLocal::NetworkDataLocal(Mle *mle):
    socket_(&RecvFrom, this) {
  mle_ = mle;
}

ThreadError NetworkDataLocal::AddOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length,
                                              uint8_t flags, bool stable) {
  RemoveOnMeshPrefix(prefix, prefix_length);

  PrefixTlv *prefix_tlv = reinterpret_cast<PrefixTlv*>(tlvs_ + length_);
  Insert(reinterpret_cast<uint8_t*>(prefix_tlv),
         sizeof(PrefixTlv) + (prefix_length+7)/8 + sizeof(BorderRouterTlv) + sizeof(BorderRouterEntry));
  prefix_tlv->Init(0, prefix_length, prefix);
  prefix_tlv->SetSubTlvsLength(sizeof(BorderRouterTlv) + sizeof(BorderRouterEntry));
  BorderRouterTlv *br_tlv = reinterpret_cast<BorderRouterTlv*>(prefix_tlv->GetSubTlvs());
  br_tlv->Init();
  br_tlv->SetLength(br_tlv->GetLength() + sizeof(BorderRouterEntry));
  br_tlv->GetEntry(0)->Init();
  br_tlv->GetEntry(0)->SetFlags(flags);

  if (stable) {
    prefix_tlv->SetStable();
    br_tlv->SetStable();
  }

  dump("add done", tlvs_, length_);
  return kThreadError_None;
}

ThreadError NetworkDataLocal::RemoveOnMeshPrefix(const uint8_t *prefix, uint8_t prefix_length) {
  ThreadError error = kThreadError_None;
  PrefixTlv *tlv;

  VerifyOrExit((tlv = FindPrefix(prefix, prefix_length)) != NULL, error = kThreadError_Error);
  VerifyOrExit(FindBorderRouter(tlv) != NULL, error = kThreadError_Error);
  Remove(reinterpret_cast<uint8_t*>(tlv), sizeof(NetworkDataTlv) + tlv->GetLength());

 exit:
  dump("remove done", tlvs_, length_);
  return error;
}

ThreadError NetworkDataLocal::UpdateRloc() {
  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(tlvs_);
       cur < reinterpret_cast<NetworkDataTlv*>(tlvs_ + length_);
       cur = cur->GetNext()) {
    switch (cur->GetType()) {
      case NetworkDataTlv::kTypePrefix:
        UpdateRloc(reinterpret_cast<PrefixTlv*>(cur));
        break;
      default:
        assert(false);
        break;
    }
  }
  return kThreadError_None;
}

ThreadError NetworkDataLocal::UpdateRloc(PrefixTlv *prefix) {
  for (NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs());
       cur < reinterpret_cast<NetworkDataTlv*>(prefix->GetSubTlvs() + prefix->GetSubTlvsLength());
       cur = cur->GetNext()) {
    switch (cur->GetType()) {
      case NetworkDataTlv::kTypeBorderRouter:
        UpdateRloc(reinterpret_cast<BorderRouterTlv*>(cur));
        break;
      default:
        assert(false);
        break;
    }
  }
  return kThreadError_None;
}

ThreadError NetworkDataLocal::UpdateRloc(BorderRouterTlv *border_router) {
  BorderRouterEntry *entry = border_router->GetEntry(0);
  entry->SetRloc(mle_->GetAddress16());
  return kThreadError_None;
}

ThreadError NetworkDataLocal::Register(const Ip6Address *destination) {
  ThreadError error = kThreadError_None;

  UpdateRloc();
  socket_.Bind(NULL);

  for (int i = 0; i < sizeof(coap_token_); i++)
    coap_token_[i] = Random::Get();

  CoapMessage coap;
  coap.SetVersion(1);
  coap.SetType(CoapMessage::kTypeConfirmable);
  coap.SetCode(CoapMessage::kCodePost);
  coap.SetMessageId(++coap_message_id_);
  coap.SetToken(coap_token_, sizeof(coap_token_));
  coap.AppendUriPathOptions("n/sd");
  coap.Finalize();

  Message *message;
  VerifyOrExit((message = Udp6::NewMessage(0)) != NULL, error = kThreadError_NoBufs);
  SuccessOrExit(error = message->Append(coap.GetHeaderBytes(), coap.GetHeaderLength()));
  SuccessOrExit(error = message->Append(tlvs_, length_));

  Ip6MessageInfo message_info;
  memset(&message_info, 0, sizeof(message_info));
  memcpy(&message_info.peer_addr, destination, sizeof(message_info.peer_addr));
  message_info.peer_port = kCoapUdpPort;
  SuccessOrExit(error = socket_.SendTo(message, &message_info));

  dprintf("Sent network data registration\n");

exit:
  if (message != NULL && error != kThreadError_None)
    Message::Free(message);
  return error;
}

void NetworkDataLocal::RecvFrom(void *context, Message *message, const Ip6MessageInfo *message_info) {
  NetworkDataLocal *obj = reinterpret_cast<NetworkDataLocal*>(context);
  obj->RecvFrom(message, message_info);
}

void NetworkDataLocal::RecvFrom(Message *message, const Ip6MessageInfo *message_info) {
  CoapMessage coap;

  SuccessOrExit(coap.FromMessage(message));
  VerifyOrExit(coap.GetType() == CoapMessage::kTypeAcknowledgment &&
               coap.GetCode() == CoapMessage::kCodeChanged &&
               coap.GetMessageId() == coap_message_id_ &&
               coap.GetTokenLength() == sizeof(coap_token_) &&
               memcmp(coap_token_, coap.GetToken(NULL), sizeof(coap_token_)) == 0, ;);

  dprintf("Network data registration acknowledged\n");

exit:
  {}
}

}  // namespace Thread

