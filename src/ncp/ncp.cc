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

#include <ncp/ncp.h>
#include <common/code_utils.h>

namespace Thread {

Ncp::Ncp():
    netif_callback_(&HandleNetifCallback, this),
    uart_(&hdlc_),
    hdlc_(&uart_, this),
    update_addresses_task_(&RunUpdateAddressesTask, this) {
  netif_.RegisterCallback(&netif_callback_);
}

ThreadError Ncp::Start() {
  Ip6::SetNcp(this);
  return hdlc_.Start();
}

ThreadError Ncp::Stop() {
  return hdlc_.Stop();
}

ThreadError Ncp::SendMessage(Message *message) {
  ThreadError error = kThreadError_None;

  SuccessOrExit(error = send_queue_.Enqueue(message));
  if (hdlc_sending_ == false) {
    hdlc_sending_ = true;
    hdlc_.SendMessage(2, message);
  }

exit:
  return error;
}

void Ncp::HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length) {
  switch (protocol) {
    case 0:
      ProcessThreadControl(buf, buf_length);
      break;
    case 2:
      Message *message;
      VerifyOrExit((message = Ip6::NewMessage(0)) != NULL, ;);
      SuccessOrExit(message->Append(buf, buf_length));
      Ip6::HandleDatagram(message, NULL, netif_.GetInterfaceId(), NULL, true);
      break;
  }

exit:
  {}
}

ThreadError Ncp::ProcessThreadControl(uint8_t *buf, uint16_t buf_length) {
  ThreadError error = kThreadError_None;
  ThreadControl thread_control;

  VerifyOrExit(thread_control__unpack(buf_length, buf, &thread_control) != NULL,
               printf("protobuf unpack error\n"); error = kThreadError_Parse);

  switch (thread_control.message_case) {
    case THREAD_CONTROL__MESSAGE_PRIMITIVE:
      ProcessPrimitive(&thread_control);
      buf_length = thread_control__pack(&thread_control, buf);
      hdlc_.Send(0, buf, buf_length);
      hdlc_sending_ = true;
      break;
    case THREAD_CONTROL__MESSAGE_STATE:
      ProcessState(&thread_control);
      buf_length = thread_control__pack(&thread_control, buf);
      hdlc_.Send(0, buf, buf_length);
      hdlc_sending_ = true;
      break;
    case THREAD_CONTROL__MESSAGE_WHITELIST:
      ProcessWhitelist(&thread_control);
      buf_length = thread_control__pack(&thread_control, buf);
      hdlc_.Send(0, buf, buf_length);
      hdlc_sending_ = true;
      break;
    case THREAD_CONTROL__MESSAGE_SCAN_REQUEST:
      ProcessScanRequest(&thread_control);
      buf_length = thread_control__pack(&thread_control, buf);
      hdlc_.Send(0, buf, buf_length);
      hdlc_sending_ = true;
      break;
    default:
      break;
  }

exit:
  return error;
}

ThreadError Ncp::ProcessPrimitive(ThreadControl *message) {
  switch (message->primitive.type) {
    case THREAD_PRIMITIVE__TYPE__THREAD_KEY:
      ProcessPrimitiveKey(message);
      break;
    case THREAD_PRIMITIVE__TYPE__THREAD_KEY_SEQUENCE:
      ProcessPrimitiveKeySequence(message);
      break;
    case THREAD_PRIMITIVE__TYPE__THREAD_MESH_LOCAL_PREFIX:
      ProcessPrimitiveMeshLocalPrefix(message);
      break;
    case THREAD_PRIMITIVE__TYPE__THREAD_MODE:
      ProcessPrimitiveMode(message);
      break;
    case THREAD_PRIMITIVE__TYPE__THREAD_STATUS:
      ProcessPrimitiveStatus(message);
      break;
    case THREAD_PRIMITIVE__TYPE__THREAD_TIMEOUT:
      ProcessPrimitiveTimeout(message);
      break;
    case THREAD_PRIMITIVE__TYPE__IEEE802154_CHANNEL:
      ProcessPrimitiveChannel(message);
      break;
    case THREAD_PRIMITIVE__TYPE__IEEE802154_PANID:
      ProcessPrimitivePanId(message);
      break;
    case THREAD_PRIMITIVE__TYPE__IEEE802154_EXTENDED_PANID:
      ProcessPrimitiveExtendedPanId(message);
      break;
    case THREAD_PRIMITIVE__TYPE__IEEE802154_NETWORK_NAME:
      ProcessPrimitiveNetworkName(message);
      break;
    case THREAD_PRIMITIVE__TYPE__IEEE802154_SHORT_ADDR:
      ProcessPrimitiveShortAddr(message);
      break;
    case THREAD_PRIMITIVE__TYPE__IEEE802154_EXT_ADDR:
      ProcessPrimitiveExtAddr(message);
      break;
    case _THREAD_PRIMITIVE__TYPE_IS_INT_SIZE:
      break;
  }
  return kThreadError_None;
}

ThreadError Ncp::ProcessPrimitiveKey(ThreadControl *message) {
  KeyManager *key_manager = netif_.GetKeyManager();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_BYTES:
      key_manager->SetMasterKey(message->primitive.bytes.data, message->primitive.bytes.len);
      break;
    default:
      break;
  }

  message->primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;

  uint8_t key_length;
  key_manager->GetMasterKey(message->primitive.bytes.data, &key_length);
  message->primitive.bytes.len = key_length;

  return kThreadError_None;
}

ThreadError Ncp::ProcessPrimitiveKeySequence(ThreadControl *message) {
  KeyManager *key_manager = netif_.GetKeyManager();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_UINT32:
      key_manager->SetCurrentKeySequence(message->primitive.uint32);
      break;
    default:
      break;
  }

  message->primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
  message->primitive.uint32 = key_manager->GetCurrentKeySequence();

  return kThreadError_None;
}

ThreadError Ncp::ProcessPrimitiveMeshLocalPrefix(ThreadControl *message) {
  MleRouter *mle = netif_.GetMle();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_BYTES:
      mle->SetMeshLocalPrefix(message->primitive.bytes.data);
      // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
      message->primitive.bytes.len = 8;
      memcpy(message->primitive.bytes.data, mle->GetMeshLocalPrefix(), 8);
      break;
    default:
      break;
  }

  return kThreadError_Parse;
}

ThreadError Ncp::ProcessPrimitiveMode(ThreadControl *message) {
  MleRouter *mle = netif_.GetMle();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_UINT32:
      mle->SetDeviceMode(message->primitive.uint32);
      // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
      message->primitive.uint32 = mle->GetDeviceMode();
      break;
    default:
      break;
  }

  return kThreadError_Parse;
}

ThreadError Ncp::ProcessPrimitiveStatus(ThreadControl *message) {
  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_BOOL:
      if (message->primitive.bool_)
        netif_.Up();
      else
        netif_.Down();
      // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_BOOL;
      message->primitive.bool_ = netif_.IsUp();
      break;
    default:
      break;
  }
  return kThreadError_None;
}

ThreadError Ncp::ProcessPrimitiveTimeout(ThreadControl *message) {
  MleRouter *mle = netif_.GetMle();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_UINT32:
      mle->SetTimeout(message->primitive.uint32);
      break;
    default:
      break;
  }

  message->primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
  message->primitive.uint32 = mle->GetTimeout();

  return kThreadError_None;
}

ThreadError Ncp::ProcessPrimitiveChannel(ThreadControl *message) {
  Mac *mac = netif_.GetMac();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_UINT32:
      mac->SetChannel(message->primitive.uint32);
      // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
      message->primitive.uint32 = mac->GetChannel();
      return kThreadError_None;
    default:
      break;
  }
  return kThreadError_Parse;
}

ThreadError Ncp::ProcessPrimitivePanId(ThreadControl *message) {
  Mac *mac = netif_.GetMac();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_UINT32:
      mac->SetPanId(message->primitive.uint32);
      // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
      message->primitive.uint32 = mac->GetPanId();
      return kThreadError_None;
    default:
      break;
  }
  return kThreadError_Parse;
}

ThreadError Ncp::ProcessPrimitiveExtendedPanId(ThreadControl *message) {
  Mac *mac = netif_.GetMac();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_BYTES:
      mac->SetExtendedPanId(message->primitive.bytes.data);
      // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
      message->primitive.bytes.len = 8;
      memcpy(message->primitive.bytes.data, mac->GetExtendedPanId(), 8);
      return kThreadError_None;
    default:
      break;
  }
  return kThreadError_Parse;
}

ThreadError Ncp::ProcessPrimitiveNetworkName(ThreadControl *message) {
  Mac *mac = netif_.GetMac();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE_BYTES:
      mac->SetNetworkName(reinterpret_cast<const char*>(message->primitive.bytes.data));
      // fall through
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
      message->primitive.bytes.len = 16;
      memcpy(message->primitive.bytes.data, mac->GetNetworkName(), 16);
      return kThreadError_None;
    default:
      break;
  }
  return kThreadError_Parse;
}

ThreadError Ncp::ProcessPrimitiveShortAddr(ThreadControl *message) {
  Mac *mac = netif_.GetMac();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_UINT32;
      message->primitive.uint32 = mac->GetAddress16();
      return kThreadError_None;
    default:
      break;
  }
  return kThreadError_Parse;
}

ThreadError Ncp::ProcessPrimitiveExtAddr(ThreadControl *message) {
  Mac *mac = netif_.GetMac();

  switch (message->primitive.value_case) {
    case THREAD_PRIMITIVE__VALUE__NOT_SET:
      message->primitive.value_case = THREAD_PRIMITIVE__VALUE_BYTES;
      message->primitive.bytes.len = 8;
      memcpy(message->primitive.bytes.data, mac->GetAddress64(), 8);
      return kThreadError_None;
    default:
      break;
  }
  return kThreadError_Parse;
}

ThreadError Ncp::ProcessState(ThreadControl *message) {
  MleRouter *mle = netif_.GetMle();

  if (message->state.has_state) {
    switch (message->state.state) {
      case THREAD_STATE__STATE__DETACHED:
        mle->BecomeDetached();
        break;
      case THREAD_STATE__STATE__CHILD:
        mle->BecomeChild(Mle::kJoinSamePartition);
        break;
      case THREAD_STATE__STATE__ROUTER:
        mle->BecomeRouter();
        break;
      case THREAD_STATE__STATE__LEADER:
        mle->BecomeLeader();
        break;
      case _THREAD_STATE__STATE_IS_INT_SIZE:
        break;
    }
  }

  message->state.has_state = true;
  switch (mle->GetDeviceState()) {
    case Mle::kDeviceStateDisabled:
    case Mle::kDeviceStateDetached:
      message->state.state = THREAD_STATE__STATE__DETACHED;
      break;
    case Mle::kDeviceStateChild:
      message->state.state = THREAD_STATE__STATE__CHILD;
      break;
    case Mle::kDeviceStateRouter:
      message->state.state = THREAD_STATE__STATE__ROUTER;
      break;
    case Mle::kDeviceStateLeader:
      message->state.state = THREAD_STATE__STATE__LEADER;
      break;
  }

  return kThreadError_None;
}

ThreadError Ncp::ProcessWhitelist(ThreadControl *message) {
  MacWhitelist *whitelist = netif_.GetMac()->GetWhitelist();

  switch (message->whitelist.type) {
    case THREAD_WHITELIST__TYPE__STATUS: {
      if (message->whitelist.has_status) {
        switch (message->whitelist.status) {
          case THREAD_WHITELIST__STATUS__DISABLE:
            whitelist->Disable();
            break;
          case THREAD_WHITELIST__STATUS__ENABLE:
            whitelist->Enable();
            break;
          case _THREAD_WHITELIST__STATUS_IS_INT_SIZE:
            break;
        }
      }

      message->whitelist.has_status = true;
      message->whitelist.status = whitelist->IsEnabled() ?
          THREAD_WHITELIST__STATUS__ENABLE : THREAD_WHITELIST__STATUS__DISABLE;
      break;
    }
    case THREAD_WHITELIST__TYPE__LIST: {
      message->whitelist.n_address = whitelist->GetMaxEntries();
      for (int i = 0; i < message->whitelist.n_address; i++) {
        message->whitelist.address[i].len = sizeof(message->whitelist.address[i].data);
        memcpy(message->whitelist.address[i].data, whitelist->GetAddress(i),
               sizeof(message->whitelist.address[i].data));
      }
      break;
    }
    case THREAD_WHITELIST__TYPE__ADD:
      whitelist->Add(reinterpret_cast<MacAddr64*>(message->whitelist.address[0].data));
      break;
    case THREAD_WHITELIST__TYPE__CLEAR:
      whitelist->Clear();
      break;
    case THREAD_WHITELIST__TYPE__DELETE:
      whitelist->Remove(reinterpret_cast<MacAddr64*>(message->whitelist.address[0].data));
      break;
    case _THREAD_WHITELIST__TYPE_IS_INT_SIZE:
      break;
  }

  return kThreadError_None;
}

ThreadError Ncp::ProcessScanRequest(ThreadControl *message) {
  uint16_t channel_mask = 0;
  uint16_t scan_interval = 0;

  if (message->scan_request.has_channel_mask) {
    channel_mask = static_cast<uint16_t>(message->scan_request.channel_mask);
  }

  if (message->scan_request.has_scan_interval_per_channel) {
    scan_interval = static_cast<uint16_t>(message->scan_request.scan_interval_per_channel);
  }

  return netif_.GetMac()->ActiveScan(scan_interval, channel_mask, &HandleActiveScanResult, this);
}

void Ncp::HandleActiveScanResult(void *context, Mac::ActiveScanResult *result) {
  Ncp *obj = reinterpret_cast<Ncp*>(context);
  obj->HandleActiveScanResult(result);
}

void Ncp::HandleActiveScanResult(Mac::ActiveScanResult *result) {
  ThreadControl message;
  size_t len;

  VerifyOrExit(hdlc_sending_ == false, ;);

  thread_control__init(&message);

  message.message_case = THREAD_CONTROL__MESSAGE_SCAN_RESULT;
  thread_scan_result__init(&message.scan_result);

  len = sizeof(result->network_name);
  memcpy(message.scan_result.network_name.data,  result->network_name, len);
  message.scan_result.network_name.len = len;

  len = sizeof(result->ext_panid);
  memcpy(message.scan_result.ext_panid.data,  result->ext_panid, len);
  message.scan_result.ext_panid.len = len;

  len = sizeof(result->ext_addr);
  memcpy(message.scan_result.ext_addr.data,  result->ext_addr, len);
  message.scan_result.ext_addr.len = len;

  message.scan_result.panid = static_cast<uint32_t>(result->panid);
  message.scan_result.channel = static_cast<uint32_t>(result->channel);
  message.scan_result.rssi = static_cast<int32_t>(result->rssi);

  uint8_t buf[512];
  int buf_length;

  buf_length = thread_control__pack(&message, buf);
  hdlc_.Send(0, buf, buf_length);
  hdlc_sending_ = true;

 exit:
  return;
}

void Ncp::HandleNetifCallback(void *context) {
  Ncp *obj = reinterpret_cast<Ncp*>(context);
  obj->update_addresses_task_.Post();
}

void Ncp::RunUpdateAddressesTask(void *context) {
  Ncp *obj = reinterpret_cast<Ncp*>(context);
  obj->RunUpdateAddressesTask();
}

void Ncp::RunUpdateAddressesTask() {
  ThreadControl message;
  thread_control__init(&message);
  message.message_case = THREAD_CONTROL__MESSAGE_ADDRESSES;

  thread_ip6_addresses__init(&message.addresses);

  for (const NetifAddress *address = netif_.GetAddresses(); address; address = address->next_) {
    int n = message.addresses.n_address;
    message.addresses.address[n].len = sizeof(message.addresses.address[n].data);
    memcpy(message.addresses.address[n].data, &address->address, sizeof(message.addresses.address[n].data));
    n++;

    message.addresses.n_address = n;
    if (n >= sizeof(message.addresses.address) / sizeof(message.addresses.address[0]))
      break;
  }

  uint8_t buf[1024];
  int buf_length;

  buf_length = thread_control__pack(&message, buf);

  hdlc_.Send(1, buf, buf_length);
  hdlc_sending_ = true;
}

void Ncp::HandleSendDone() {
  hdlc_sending_ = false;

  if (send_queue_.GetHead() != NULL) {
      hdlc_.SendMessage(2, send_queue_.GetHead());
      hdlc_sending_ = true;
  }
}

void Ncp::HandleSendMessageDone() {
  Message *message = send_queue_.GetHead();
  send_queue_.Dequeue(message);
  Message::Free(message);
  HandleSendDone();
}

}  // namespace Thread
