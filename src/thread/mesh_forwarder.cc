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

#include <thread/mesh_forwarder.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <common/message.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <net/ip6.h>
#include <net/netif.h>
#include <net/udp6.h>
#include <thread/mle_router.h>
#include <thread/thread_netif.h>

using Thread::Encoding::BigEndian::HostSwap16;

namespace Thread {

MeshForwarder::MeshForwarder(AddressResolver *address_resolver, Mac *mac, MleRouter *mle, Netif *netif,
                             NetworkDataLeader *network_data):
    lowpan_(network_data),
    mac_receiver_(&HandleReceivedFrame, this),
    mac_sender_(&HandleFrameRequest, &HandleSentFrame, this),
    poll_timer_(&HandlePollTimer, this),
    reassembly_timer_(&HandleReassemblyTimer, this),
    schedule_transmission_task_(ScheduleTransmissionTask, this) {
  address_resolver_ = address_resolver;
  mac_ = mac;
  mle_ = mle;
  netif_ = netif;
  network_data_ = network_data;
  frag_tag_ = Random::Get();
}

ThreadError MeshForwarder::Start() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(enabled_ == false, error = kThreadError_Busy);

  mac_->RegisterReceiver(&mac_receiver_);
  SuccessOrExit(error = mac_->Start());

  enabled_ = true;

exit:
  return error;
}

ThreadError MeshForwarder::Stop() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(enabled_ == true, error = kThreadError_Busy);

  poll_timer_.Stop();
  reassembly_timer_.Stop();

  Message *message;
  while ((message = send_queue_.GetHead()) != NULL) {
    send_queue_.Dequeue(message);
    Message::Free(message);
  }

  while ((message = reassembly_list_.GetHead()) != NULL) {
    reassembly_list_.Dequeue(message);
    Message::Free(message);
  }

  enabled_ = false;

  SuccessOrExit(error = mac_->Stop());

exit:
  return error;
}

const MacAddr64 *MeshForwarder::GetAddress64() const {
  return mac_->GetAddress64();
}

MacAddr16 MeshForwarder::GetAddress16() const {
  return mac_->GetAddress16();
}

ThreadError MeshForwarder::SetAddress16(MacAddr16 address16) {
  mac_->SetAddress16(address16);
  return kThreadError_None;
}

void MeshForwarder::HandleResolved(const Ip6Address *eid) {
  Message *cur, *next;

  for (cur = resolving_queue_.GetHead(); cur; cur = next) {
    next = resolving_queue_.GetNext(cur);

    if (cur->GetType() != Message::kTypeIp6)
      continue;

    Ip6Address ip6_dst;
    cur->Read(offsetof(Ip6Header, ip6_dst), sizeof(ip6_dst), &ip6_dst);

    if (memcmp(&ip6_dst, eid, sizeof(ip6_dst)) == 0) {
      resolving_queue_.Dequeue(cur);
      send_queue_.Enqueue(cur);
    }
  }

  schedule_transmission_task_.Post();
}

void MeshForwarder::ScheduleTransmissionTask(void *context) {
  MeshForwarder *obj = reinterpret_cast<MeshForwarder*>(context);
  obj->ScheduleTransmissionTask();
}

void MeshForwarder::ScheduleTransmissionTask() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(send_busy_ == false, error = kThreadError_Busy);

  uint8_t num_children;
  Child *children;
  children = mle_->GetChildren(&num_children);
  for (int i = 0; i < num_children; i++) {
    if (children[i].state == Child::kStateValid &&
        children[i].data_request &&
        (send_message_ = GetIndirectTransmission(&children[i])) != NULL) {
      mac_->SendFrameRequest(&mac_sender_);
      ExitNow();
    }
  }

  if ((send_message_ = GetDirectTransmission()) != NULL) {
    mac_->SendFrameRequest(&mac_sender_);
    ExitNow();
  }

exit:
  {}
}

ThreadError MeshForwarder::SendMessage(Message *message) {
  ThreadError error = kThreadError_None;
  Neighbor *neighbor;

  switch (message->GetType()) {
    case Message::kTypeIp6: {
      Ip6Header ip6_header;
      message->Read(0, sizeof(ip6_header), &ip6_header);
      if (!memcmp(&ip6_header.ip6_dst, mle_->GetLinkLocalAllThreadNodesAddress(), sizeof(ip6_header.ip6_dst)) ||
          !memcmp(&ip6_header.ip6_dst, mle_->GetRealmLocalAllThreadNodesAddress(), sizeof(ip6_header.ip6_dst))) {
        // schedule direct transmission
        message->SetDirectTransmission();

        // destined for all sleepy children
        uint8_t num_children;
        Child *children = mle_->GetChildren(&num_children);
        for (int i = 0; i < num_children; i++) {
          if (children[i].state == Neighbor::kStateValid && (children[i].mode & Mle::kModeRxOnWhenIdle) == 0)
            message->SetChildMask(i);
        }
      } else if ((neighbor = mle_->GetNeighbor(&ip6_header.ip6_dst)) != NULL &&
                 (neighbor->mode & Mle::kModeRxOnWhenIdle) == 0) {
        // destined for a sleepy child
        message->SetChildMask(mle_->GetChildIndex(reinterpret_cast<Child*>(neighbor)));
      } else {
        // schedule direct transmission
        message->SetDirectTransmission();
      }
      break;
    }

    case Message::kTypeMesh: {
      MeshHeader mesh_header;
      message->Read(0, sizeof(mesh_header), &mesh_header);

      if ((neighbor = mle_->GetNeighbor(mesh_header.GetDestination())) != NULL &&
          (neighbor->mode & Mle::kModeRxOnWhenIdle) == 0) {
        // destined for a sleepy child
        message->SetChildMask(mle_->GetChildIndex(reinterpret_cast<Child*>(neighbor)));
      } else {
        // not destined for a sleepy child
        message->SetDirectTransmission();
      }
      break;
    }

    case Message::kTypePoll: {
      message->SetDirectTransmission();
      break;
    }
  }

  message->SetOffset(0);
  SuccessOrExit(error = send_queue_.Enqueue(message));
  schedule_transmission_task_.Post();

exit:
  return error;
}

void MeshForwarder::MoveToResolving(const Ip6Address *destination) {
  Message *cur, *next;

  for (cur = send_queue_.GetHead(); cur; cur = next) {
    next = send_queue_.GetNext(cur);

    if (cur->GetType() != Message::kTypeIp6)
      continue;

    Ip6Address ip6_dst;
    cur->Read(offsetof(Ip6Header, ip6_dst), sizeof(ip6_dst), &ip6_dst);

    if (memcmp(&ip6_dst, destination, sizeof(ip6_dst)) == 0) {
      send_queue_.Dequeue(cur);
      resolving_queue_.Enqueue(cur);
    }
  }
}

Message *MeshForwarder::GetDirectTransmission() {
  Message *cur_message, *next_message;
  ThreadError error;

  for (cur_message = send_queue_.GetHead(); cur_message; cur_message = next_message) {
    next_message = send_queue_.GetNext(cur_message);

    if (cur_message->GetDirectTransmission() == false)
      continue;

    switch (cur_message->GetType()) {
      case Message::kTypeIp6:
        error = UpdateIp6Route(cur_message);
        break;
      case Message::kTypeMesh:
        error = UpdateMeshRoute(cur_message);
        break;
      case Message::kTypePoll:
        ExitNow();
        break;
    }

    switch (error) {
      case kThreadError_None:
        ExitNow();
        break;
      case kThreadError_LeaseQuery:
        Ip6Address ip6_dst;
        cur_message->Read(offsetof(Ip6Header, ip6_dst), sizeof(ip6_dst), &ip6_dst);
        MoveToResolving(&ip6_dst);
        continue;
      case kThreadError_Drop:
      case kThreadError_NoBufs:
        send_queue_.Dequeue(cur_message);
        Message::Free(cur_message);
        continue;
      default:
        dprintf("error = %d\n", error);
        assert(false);
        break;
    }
  }

exit:
  return cur_message;
}

Message *MeshForwarder::GetIndirectTransmission(Child *child) {
  Message *message = NULL;
  int child_index = mle_->GetChildIndex(child);

  for (message = send_queue_.GetHead(); message; message = send_queue_.GetNext(message)) {
    if (message->GetChildMask(child_index))
      break;
  }

  VerifyOrExit(message != NULL, ;);

  switch (message->GetType()) {
    case Message::kTypeIp6: {
      Ip6Header ip6_header;
      message->Read(0, sizeof(ip6_header), &ip6_header);

      MacAddress macaddr;
      GetMacSourceAddress(&ip6_header.ip6_src, &macaddr);
      message->SetMacSource(&macaddr);

      if (ip6_header.ip6_dst.IsLinkLocal() || ip6_header.ip6_dst.IsMulticast()) {
        GetMacDestinationAddress(&ip6_header.ip6_dst, &macaddr);
      } else {
        macaddr.length = 2;
        macaddr.address16 = child->valid.address16;
      }
      message->SetMacDestination(&macaddr);
      break;
    }

    case Message::kTypeMesh: {
      MeshHeader mesh_header;
      message->Read(0, sizeof(mesh_header), &mesh_header);

      message->SetMacDestination(mesh_header.GetDestination());
      message->SetMacSource(mac_->GetAddress16());
      message->SetMeshDestination(mesh_header.GetDestination());
      message->SetMeshSource(mesh_header.GetSource());
      break;
    }

    default:
      assert(false);
      break;
  }

exit:
  return message;
}

ThreadError MeshForwarder::UpdateMeshRoute(Message *message) {
  ThreadError error = kThreadError_None;
  MeshHeader mesh_header;

  message->Read(0, sizeof(mesh_header), &mesh_header);

  Neighbor *neighbor;
  if ((neighbor = mle_->GetNeighbor(mesh_header.GetDestination())) == NULL) {
    uint16_t next_hop;
    VerifyOrExit((next_hop = mle_->GetNextHop(mesh_header.GetDestination())) != MacFrame::kShortAddrInvalid,
                 error = kThreadError_Drop);
    VerifyOrExit((neighbor = mle_->GetNeighbor(next_hop)) != NULL, error = kThreadError_Drop);
  }

  // dprintf("MESH ROUTE = %02x %02x\n", mesh_header.GetDestination(), neighbor->valid.address16);
  message->SetMacDestination(neighbor->valid.address16);
  message->SetMacSource(mac_->GetAddress16());
  message->SetMeshDestination(mesh_header.GetDestination());
  message->SetMeshSource(mesh_header.GetSource());

exit:
  return error;
}

ThreadError MeshForwarder::UpdateIp6Route(Message *message) {
  ThreadError error = kThreadError_None;
  Ip6Header ip6_header;
  MacAddress meshdst, meshsrc;
  MacAddress macdst, macsrc;
  bool mesh_header = false;
  Neighbor *neighbor;

  message->Read(0, sizeof(ip6_header), &ip6_header);

  if (ip6_header.ip6_dst.IsLinkLocal() || ip6_header.ip6_dst.IsMulticast()) {
    GetMacDestinationAddress(&ip6_header.ip6_dst, &meshdst);
    macdst = meshdst;
    GetMacSourceAddress(&ip6_header.ip6_src, &meshsrc);
    macsrc = meshsrc;
  } else if (mle_->GetDeviceState() != Mle::kDeviceStateDetached) {
    // non-link-local unicast
    if (mle_->GetDeviceMode() & Mle::kModeFFD) {
      // FFD - peform full routing
      if (mle_->IsRoutingLocator(&ip6_header.ip6_dst)) {
        meshdst.length = 2;
        meshdst.address16 = HostSwap16(ip6_header.ip6_dst.s6_addr16[7]);
      } else if ((neighbor = mle_->GetNeighbor(&ip6_header.ip6_dst)) != NULL) {
        meshdst.length = 2;
        meshdst.address16 = neighbor->valid.address16;
      } else if (network_data_->IsOnMesh(ip6_header.ip6_dst)) {
        meshdst.length = 2;
        SuccessOrExit(error = address_resolver_->Resolve(&ip6_header.ip6_dst, &meshdst.address16));
      } else {
        meshdst.length = 2;
        network_data_->RouteLookup(ip6_header.ip6_src, ip6_header.ip6_dst, &meshdst.address16);
        assert(meshdst.address16 != MacFrame::kShortAddrInvalid);
      }
    } else {
      // RFD - send to parent
      meshdst.length = 2;
      meshdst.address16 = mle_->GetNextHop(MacFrame::kShortAddrBroadcast);
    }

    if ((mle_->GetDeviceMode() & Mle::kModeFFD) == 0 ||
        (mle_->GetDeviceState() != Mle::kDeviceStateChild && (neighbor = mle_->GetNeighbor(&meshdst)) != NULL)) {
      // destination is neighbor
      macdst = meshdst;
      if (netif_->IsAddress(&ip6_header.ip6_src)) {
        GetMacSourceAddress(&ip6_header.ip6_src, &meshsrc);
        macsrc = meshsrc;
      } else {
        meshsrc.length = 2;
        meshsrc.address16 = mac_->GetAddress16();
        assert(meshsrc.address16 != MacFrame::kShortAddrInvalid);
        macsrc = meshsrc;
      }
    } else {
      // destination is not neighbor
      meshsrc.length = 2;
      meshsrc.address16 = mac_->GetAddress16();

      SuccessOrExit(error = mle_->CheckReachability(meshsrc.address16, meshdst.address16, &ip6_header));

      macdst.length = 2;
      macdst.address16 = mle_->GetNextHop(meshdst.address16);
      macsrc = meshsrc;
      mesh_header = true;
    }
  } else {
    assert(false);
    ExitNow(error = kThreadError_Drop);
  }

  message->SetMeshHeaderEnable(mesh_header);
  if (mesh_header) {
    assert(meshdst.length == 2);
    message->SetMeshDestination(meshdst.address16);
    assert(meshsrc.length == 2);
    message->SetMeshSource(meshsrc.address16);
  }
  message->SetMacDestination(&macdst);
  message->SetMacSource(&macsrc);

exit:
  return error;
}

bool MeshForwarder::GetRxOnWhenIdle() {
  return mac_->GetRxOnWhenIdle();
}

ThreadError MeshForwarder::SetRxOnWhenIdle(bool rx_on_when_idle) {
  ThreadError error;

  SuccessOrExit(error = mac_->SetRxOnWhenIdle(rx_on_when_idle));

  if (rx_on_when_idle)
    poll_timer_.Stop();
  else
    poll_timer_.Start(poll_period_);

exit:
  return error;
}

ThreadError MeshForwarder::SetPollPeriod(uint32_t period) {
  if (mac_->GetRxOnWhenIdle() == false && poll_period_ != period)
    poll_timer_.Start(period);
  poll_period_ = period;
  return kThreadError_None;
}

void MeshForwarder::HandlePollTimer(void *context) {
  MeshForwarder *obj = reinterpret_cast<MeshForwarder*>(context);
  obj->HandlePollTimer();
}

void MeshForwarder::HandlePollTimer() {
  Message *message;

  if ((message = Message::New(Message::kTypePoll, 0)) != NULL) {
    SendMessage(message);
    dprintf("Sent poll\n");
  }

  poll_timer_.Start(poll_period_);
}

ThreadError MeshForwarder::GetMacSourceAddress(const Ip6Address *ipaddr, MacAddress *macaddr) {
  assert(!ipaddr->IsMulticast());

  macaddr->length = 8;
  memcpy(&macaddr->address64, ipaddr->s6_addr + 8, sizeof(macaddr->address64));
  macaddr->address64.bytes[0] ^= 0x02;

  if (memcmp(&macaddr->address64, mac_->GetAddress64(), sizeof(macaddr->address64)) != 0) {
    macaddr->length = 2;
    macaddr->address16 = mac_->GetAddress16();
  }

  return kThreadError_None;
}

ThreadError MeshForwarder::GetMacDestinationAddress(const Ip6Address *ipaddr, MacAddress *macaddr) {
  if (ipaddr->IsMulticast()) {
    macaddr->length = 2;
    macaddr->address16 = MacFrame::kShortAddrBroadcast;
  } else if (ipaddr->s6_addr16[0] == HostSwap16(0xfe80) &&
             ipaddr->s6_addr16[1] == HostSwap16(0x0000) &&
             ipaddr->s6_addr16[2] == HostSwap16(0x0000) &&
             ipaddr->s6_addr16[3] == HostSwap16(0x0000) &&
             ipaddr->s6_addr16[4] == HostSwap16(0x0000) &&
             ipaddr->s6_addr16[5] == HostSwap16(0x00ff) &&
             ipaddr->s6_addr16[6] == HostSwap16(0xfe00)) {
    macaddr->length = 2;
    macaddr->address16 = HostSwap16(ipaddr->s6_addr16[7]);
  } else if (mle_->IsRoutingLocator(ipaddr)) {
    macaddr->length = 2;
    macaddr->address16 = HostSwap16(ipaddr->s6_addr16[7]);
  } else {
    macaddr->length = 8;
    memcpy(&macaddr->address64, ipaddr->s6_addr + 8, sizeof(macaddr->address64));
    macaddr->address64.bytes[0] ^= 0x02;
  }

  return kThreadError_None;
}

ThreadError MeshForwarder::HandleFrameRequest(void *context, MacFrame *frame) {
  MeshForwarder *obj = reinterpret_cast<MeshForwarder*>(context);
  return obj->HandleFrameRequest(frame);
}

ThreadError MeshForwarder::HandleFrameRequest(MacFrame *frame) {
  send_busy_ = true;
  assert(send_message_ != NULL);

  switch (send_message_->GetType()) {
    case Message::kTypeIp6:
      SendFragment(send_message_, frame);
      assert(frame->GetLength() != 7);
      break;
    case Message::kTypeMesh:
      SendMesh(send_message_, frame);
      break;
    case Message::kTypePoll:
      SendPoll(send_message_, frame);
      break;
  }

#if 0
  dump("sent frame", frame->GetHeader(), frame->GetLength());
#endif

  return kThreadError_None;
}

ThreadError MeshForwarder::SendPoll(Message *message, MacFrame *frame) {
  MacAddress macsrc;

  macsrc.address16 = mac_->GetAddress16();
  if (macsrc.address16 != MacFrame::kShortAddrInvalid) {
    macsrc.length = 2;
  } else {
    macsrc.length = 8;
    memcpy(&macsrc.address64, mac_->GetAddress64(), sizeof(macsrc.address64));
  }

  // initialize MAC header
  uint16_t fcf = MacFrame::kFcfFrameMacCmd | MacFrame::kFcfPanidCompression | MacFrame::kFcfFrameVersion2006;
  if (macsrc.length == 2)
    fcf |= MacFrame::kFcfDstAddrShort | MacFrame::kFcfSrcAddrShort;
  else
    fcf |= MacFrame::kFcfDstAddrExt | MacFrame::kFcfSrcAddrExt;
  fcf |= MacFrame::kFcfAckRequest | MacFrame::kFcfSecurityEnabled;

  frame->InitMacHeader(fcf, MacFrame::kKeyIdMode1 | MacFrame::kSecEncMic32);
  frame->SetDstPanId(mac_->GetPanId());

  Neighbor *neighbor;
  neighbor = mle_->GetParent();
  assert(neighbor != NULL);

  if (macsrc.length == 2) {
    frame->SetDstAddr(neighbor->valid.address16);
    frame->SetSrcAddr(macsrc.address16);
  } else {
    frame->SetDstAddr(&neighbor->mac_addr);
    frame->SetSrcAddr(&macsrc.address64);
  }

  frame->SetCommandId(MacFrame::kMacCmdDataRequest);

  message_next_offset_ = message->GetLength();

  return kThreadError_None;
}

ThreadError MeshForwarder::SendMesh(Message *message, MacFrame *frame) {
  MacAddress macsrc, macdst;

  message->GetMacDestination(&macdst);
  message->GetMacSource(&macsrc);
  assert(macdst.length == 2 && macsrc.length == 2);

  // initialize MAC header
  uint16_t fcf = MacFrame::kFcfFrameData | MacFrame::kFcfPanidCompression | MacFrame::kFcfFrameVersion2006;
  fcf |= MacFrame::kFcfDstAddrShort | MacFrame::kFcfSrcAddrShort;
  fcf |= MacFrame::kFcfAckRequest | MacFrame::kFcfSecurityEnabled;

  frame->InitMacHeader(fcf, MacFrame::kKeyIdMode1 | MacFrame::kSecEncMic32);
  frame->SetDstPanId(mac_->GetPanId());
  frame->SetDstAddr(macdst.address16);
  frame->SetSrcAddr(macsrc.address16);

  // write payload
  uint8_t *payload;
  payload = frame->GetPayload();
  if (message->GetLength() > frame->GetMaxPayloadLength())
    fprintf(stderr, "%d %d\n", message->GetLength(), frame->GetMaxPayloadLength());
  assert(message->GetLength() <= frame->GetMaxPayloadLength());
  message->Read(0, message->GetLength(), payload);
  frame->SetPayloadLength(message->GetLength());

  message_next_offset_ = message->GetLength();

  return kThreadError_None;
}

ThreadError MeshForwarder::SendFragment(Message *message, MacFrame *frame) {
  MacAddress meshdst, meshsrc;
  MacAddress macdst, macsrc;
  bool mesh_header;

  message->GetMacDestination(&macdst);
  message->GetMacSource(&macsrc);
  if ((mesh_header = message->IsMeshHeaderEnabled())) {
    meshsrc.length = 2;
    meshsrc.address16 = message->GetMeshSource();
    meshdst.length = 2;
    meshdst.address16 = message->GetMeshDestination();
  } else {
    meshdst = macdst;
    meshsrc = macsrc;
  }

  // initialize MAC header
  uint16_t fcf;
  fcf = MacFrame::kFcfFrameData | MacFrame::kFcfPanidCompression | MacFrame::kFcfFrameVersion2006;
  fcf |= (macdst.length == 2) ? MacFrame::kFcfDstAddrShort : MacFrame::kFcfDstAddrExt;
  fcf |= (macsrc.length == 2) ? MacFrame::kFcfSrcAddrShort : MacFrame::kFcfSrcAddrExt;

  // all unicast frames request ACK
  if (macdst.length == 8 || macdst.address16 != MacFrame::kShortAddrBroadcast)
    fcf |= MacFrame::kFcfAckRequest;

  fcf |= MacFrame::kFcfSecurityEnabled;

  Ip6Header ip6_header;
  message->Read(0, sizeof(ip6_header), &ip6_header);
  if (ip6_header.ip6_nxt == IPPROTO_UDP) {
    UdpHeader udp_header;
    message->Read(sizeof(ip6_header), sizeof(udp_header), &udp_header);
    if (udp_header.dest == HostSwap16(Mle::kUdpPort))
      fcf &= ~MacFrame::kFcfSecurityEnabled;
  }

  frame->InitMacHeader(fcf, MacFrame::kKeyIdMode1 | MacFrame::kSecEncMic32);
  frame->SetDstPanId(mac_->GetPanId());
  if (macdst.length == 2)
    frame->SetDstAddr(macdst.address16);
  else
    frame->SetDstAddr(&macdst.address64);
  if (macsrc.length == 2)
    frame->SetSrcAddr(macsrc.address16);
  else
    frame->SetSrcAddr(&macsrc.address64);

  uint8_t *payload;
  payload = frame->GetPayload();

  int header_length;
  header_length = 0;

  // initialize Mesh header
  if (mesh_header) {
    MeshHeader *mesh_header = reinterpret_cast<MeshHeader*>(payload);
    mesh_header->Init();
    mesh_header->SetHopsLeft(Lowpan::kHopsLeft);
    mesh_header->SetSource(meshsrc.address16);
    mesh_header->SetDestination(meshdst.address16);
    payload += mesh_header->GetHeaderLength();
    header_length += mesh_header->GetHeaderLength();
  }

  int payload_length;

  // copy IPv6 Header
  if (message->GetOffset() == 0) {
    int hc_length;
    hc_length = lowpan_.Compress(message, &meshsrc, &meshdst, payload);
    assert(hc_length > 0);
    header_length += hc_length;

    payload_length = message->GetLength() - message->GetOffset();

    uint16_t fragment_length;
    fragment_length = frame->GetMaxPayloadLength() - header_length;
    if (payload_length > fragment_length) {
      // write Fragment header
      message->SetDatagramTag(frag_tag_++);
      memmove(payload + 4, payload, header_length);

      payload_length = (frame->GetMaxPayloadLength() - header_length - 4) & ~0x7;

      FragmentHeader *fragment_header = reinterpret_cast<FragmentHeader*>(payload);
      fragment_header->Init();
      fragment_header->SetSize(message->GetLength());
      fragment_header->SetTag(message->GetDatagramTag());

      payload += fragment_header->GetHeaderLength();
      header_length += fragment_header->GetHeaderLength();
    }
    payload += hc_length;

    // copy IPv6 Payload
    message->Read(message->GetOffset(), payload_length, payload);
    frame->SetPayloadLength(header_length + payload_length);

    message_next_offset_ = message->GetOffset() + payload_length;
    message->SetOffset(0);
  } else {
    payload_length = message->GetLength() - message->GetOffset();

    // write Fragment header
    FragmentHeader *fragment_header = reinterpret_cast<FragmentHeader*>(payload);
    fragment_header->Init();
    fragment_header->SetSize(message->GetLength());
    fragment_header->SetTag(message->GetDatagramTag());
    fragment_header->SetOffset(message->GetOffset());

    payload += fragment_header->GetHeaderLength();
    header_length += fragment_header->GetHeaderLength();

    uint16_t fragment_length = (frame->GetMaxPayloadLength() - header_length) & ~0x7;
    if (payload_length > fragment_length)
      payload_length = fragment_length;

    // copy IPv6 Payload
    message->Read(message->GetOffset(), payload_length, payload);
    frame->SetPayloadLength(header_length + payload_length);

    message_next_offset_ = message->GetOffset() + payload_length;
  }

  if (message_next_offset_ < message->GetLength())
    frame->SetFramePending(true);

  return kThreadError_None;
}

void MeshForwarder::HandleSentFrame(void *context, MacFrame *frame) {
  MeshForwarder *obj = reinterpret_cast<MeshForwarder*>(context);
  obj->HandleSentFrame(frame);
}

void MeshForwarder::HandleSentFrame(MacFrame *frame) {
  send_busy_ = false;

  if (!enabled_)
    return;

  send_message_->SetOffset(message_next_offset_);

  MacAddress macdst;
  frame->GetDstAddr(&macdst);

  dprintf("sent frame %d %d\n", message_next_offset_, send_message_->GetLength());

  Child *child;
  if ((child = mle_->GetChild(&macdst)) != NULL) {
    child->data_request = false;
    if (message_next_offset_ < send_message_->GetLength()) {
      child->fragment_offset = message_next_offset_;
    } else {
      child->fragment_offset = 0;
      send_message_->ClearChildMask(mle_->GetChildIndex(child));
    }
  }

  if (send_message_->GetDirectTransmission()) {
    if (message_next_offset_ < send_message_->GetLength())
      send_message_->SetOffset(message_next_offset_);
    else
      send_message_->ClearDirectTransmission();
  }

  if (send_message_->GetDirectTransmission() == false && send_message_->IsChildPending() == false) {
    send_queue_.Dequeue(send_message_);
    Message::Free(send_message_);
  }

  schedule_transmission_task_.Post();
}

void MeshForwarder::HandleReceivedFrame(void *context, MacFrame *frame, ThreadError error) {
  MeshForwarder *obj = reinterpret_cast<MeshForwarder*>(context);
  obj->HandleReceivedFrame(frame, error);
}

void MeshForwarder::HandleReceivedFrame(MacFrame *frame, ThreadError error) {
  ThreadMessageInfo message_info;
  MacAddress macdst;
  MacAddress macsrc;
  uint8_t *payload;
  uint8_t payload_length;

#if 0
  dump("received frame", frame->GetHeader(), frame->GetLength());
#endif

  if (!enabled_)
    ExitNow();

  SuccessOrExit(frame->GetSrcAddr(&macsrc));

  if (error == kThreadError_Security) {
    Ip6Address destination;
    memset(&destination, 0, sizeof(destination));
    destination.s6_addr16[0] = HostSwap16(0xfe80);

    switch (macsrc.length) {
      case 2:
        destination.s6_addr16[5] = HostSwap16(0x00ff);
        destination.s6_addr16[6] = HostSwap16(0xfe00);
        destination.s6_addr16[7] = HostSwap16(macsrc.address16);
        break;
      case 8:
        memcpy(destination.s6_addr + 8, &macsrc.address64, sizeof(macsrc.address64));
        break;
      default:
        ExitNow();
    }

    mle_->SendLinkReject(&destination);
    ExitNow();
  }

  SuccessOrExit(frame->GetDstAddr(&macdst));
  message_info.link_margin = frame->GetPower() - -100;

  payload = frame->GetPayload();
  payload_length = frame->GetPayloadLength();

  if (poll_timer_.IsRunning() && frame->GetFramePending())
    HandlePollTimer();

  switch (frame->GetType()) {
    case MacFrame::kFcfFrameData:
      if ((payload[0] & MeshHeader::kDispatchMask) == MeshHeader::kDispatch) {
        HandleMesh(payload, payload_length, &macsrc, &macdst, &message_info);
      } else if ((payload[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch) {
        HandleFragment(payload, payload_length, &macsrc, &macdst, &message_info);
      } else if ((payload[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8)) {
        HandleLowpanHC(payload, payload_length, &macsrc, &macdst, &message_info);
      }
      break;
    case MacFrame::kFcfFrameMacCmd:
      uint8_t command_id;
      frame->GetCommandId(&command_id);
      if (command_id == MacFrame::kMacCmdDataRequest)
        HandleDataRequest(&macsrc);
      break;
  }

exit:
  {}
}

void MeshForwarder::HandleMesh(uint8_t *frame, uint8_t frame_length, MacAddress *macsrc, MacAddress *macdst,
                               ThreadMessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Message *message = NULL;
  MacAddress meshdst;
  MacAddress meshsrc;

  MeshHeader *mesh_header = reinterpret_cast<MeshHeader*>(frame);
  VerifyOrExit(mesh_header->IsValid(), error = kThreadError_Drop);

  meshsrc.length = 2;
  meshsrc.address16 = mesh_header->GetSource();
  meshdst.length = 2;
  meshdst.address16 = mesh_header->GetDestination();

  if (meshdst.address16 == mac_->GetAddress16()) {
    frame += 5;
    frame_length -= 5;
    if ((frame[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch) {
      HandleFragment(frame, frame_length, &meshsrc, &meshdst, message_info);
    } else if ((frame[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8)) {
      HandleLowpanHC(frame, frame_length, &meshsrc, &meshdst, message_info);
    } else {
      ExitNow();
    }
  } else if (mesh_header->GetHopsLeft() > 0) {
    SuccessOrExit(error = CheckReachability(frame, frame_length, &meshsrc, &meshdst));

    mesh_header->SetHopsLeft(mesh_header->GetHopsLeft() - 1);

    VerifyOrExit((message = Message::New(Message::kTypeMesh, 0)) != NULL, error = kThreadError_Drop);
    SuccessOrExit(error = message->SetLength(frame_length));
    message->Write(0, frame_length, frame);

    SendMessage(message);
  }

exit:
  if (error != kThreadError_None && message != NULL)
    Message::Free(message);
}

ThreadError MeshForwarder::CheckReachability(uint8_t *frame, uint8_t frame_length,
                                             MacAddress *meshsrc, MacAddress *meshdst) {
  ThreadError error = kThreadError_None;

  // skip mesh header
  frame += 5;

  // skip fragment header
  if ((frame[0] & FragmentHeader::kDispatchMask) == FragmentHeader::kDispatch) {
    VerifyOrExit((frame[0] & FragmentHeader::kOffset) == 0, ;);
    frame += 4;
  }

  // only process IPv6 packets
  VerifyOrExit((frame[0] & (Lowpan::kHcDispatchMask >> 8)) == (Lowpan::kHcDispatch >> 8), ;);

  Ip6Header ip6_header;
  lowpan_.DecompressBaseHeader(&ip6_header, meshsrc, meshdst, frame);

  error = mle_->CheckReachability(meshsrc->address16, meshdst->address16, &ip6_header);

exit:
  return error;
}

void MeshForwarder::HandleFragment(uint8_t *frame, uint8_t frame_length, MacAddress *macsrc, MacAddress *macdst,
                                   ThreadMessageInfo *message_info) {
  FragmentHeader *fragment_header = reinterpret_cast<FragmentHeader*>(frame);
  uint16_t datagram_length = fragment_header->GetSize();
  uint16_t datagram_tag = fragment_header->GetTag();
  Message *message;

  if (fragment_header->GetOffset() == 0) {
    int header_length;

    frame += 4;
    frame_length -= 4;

    VerifyOrExit((message = Message::New(Message::kTypeIp6, 0)) != NULL, ;);
    header_length = lowpan_.Decompress(message, macsrc, macdst, frame, frame_length, datagram_length);
    VerifyOrExit(header_length > 0, Message::Free(message));
    frame += header_length;
    frame_length -= header_length;

    VerifyOrExit(message->SetLength(datagram_length) == kThreadError_None, Message::Free(message));
    datagram_length = HostSwap16(datagram_length - sizeof(Ip6Header));
    message->Write(offsetof(Ip6Header, ip6_plen), sizeof(datagram_length), &datagram_length);
    message->SetDatagramTag(datagram_tag);
    message->SetTimeout(kReassemblyTimeout);

    reassembly_list_.Enqueue(message);
    if (!reassembly_timer_.IsRunning())
      reassembly_timer_.Start(1000);
  } else {
    frame += 5;
    frame_length -= 5;

    for (message = reassembly_list_.GetHead(); message; message = reassembly_list_.GetNext(message)) {
      if (message->GetLength() == datagram_length &&
          message->GetDatagramTag() == datagram_tag &&
          message->GetOffset() == fragment_header->GetOffset())
        break;
    }

    VerifyOrExit(message != NULL, ;);
  }

  assert(message != NULL);

  // copy Fragment
  message->Write(message->GetOffset(), frame_length, frame);
  message->MoveOffset(frame_length);
  VerifyOrExit(message->GetOffset() >= message->GetLength(), ;);

  reassembly_list_.Dequeue(message);
  Ip6::HandleDatagram(message, netif_, netif_->GetInterfaceId(), message_info, false);

exit:
  {}
}

void MeshForwarder::HandleReassemblyTimer(void *context) {
  MeshForwarder *obj = reinterpret_cast<MeshForwarder*>(context);
  obj->HandleReassemblyTimer();
}

void MeshForwarder::HandleReassemblyTimer() {
  Message *next = NULL;

  for (Message *message = reassembly_list_.GetHead(); message; message = next) {
    next = reassembly_list_.GetNext(message);

    uint8_t timeout = message->GetTimeout();
    if (timeout > 0) {
      message->SetTimeout(timeout - 1);
    } else {
      reassembly_list_.Dequeue(message);
      Message::Free(message);
    }
  }

  if (reassembly_list_.GetHead() != NULL)
    reassembly_timer_.Start(1000);
}

void MeshForwarder::HandleLowpanHC(uint8_t *frame, uint8_t frame_length, MacAddress *macsrc, MacAddress *macdst,
                                   ThreadMessageInfo *message_info) {
  ThreadError error = kThreadError_None;
  Message *message;
  int header_length;
  uint16_t ip6_payload_length;

  VerifyOrExit((message = Message::New(Message::kTypeIp6, 0)) != NULL, ;);

  header_length = lowpan_.Decompress(message, macsrc, macdst, frame, frame_length, 0);
  VerifyOrExit(header_length > 0, ;);
  frame += header_length;
  frame_length -= header_length;

  SuccessOrExit(error = message->SetLength(message->GetLength() + frame_length));

  ip6_payload_length = HostSwap16(message->GetLength() - sizeof(Ip6Header));
  message->Write(offsetof(Ip6Header, ip6_plen), sizeof(ip6_payload_length), &ip6_payload_length);

  message->Write(message->GetOffset(), frame_length, frame);
  Ip6::HandleDatagram(message, netif_, netif_->GetInterfaceId(), message_info, false);

exit:
  if (error != kThreadError_None)
    Message::Free(message);
}

void MeshForwarder::UpdateFramePending() {
}

void MeshForwarder::HandleDataRequest(MacAddress *macsrc) {
  Neighbor *neighbor;
  int child_index;

  assert(mle_->GetDeviceState() != Mle::kDeviceStateDetached);

  VerifyOrExit((neighbor = mle_->GetNeighbor(macsrc)) != NULL, ;);
  neighbor->last_heard = Timer::GetNow();

  mle_->HandleMacDataRequest(reinterpret_cast<Child*>(neighbor));
  child_index = mle_->GetChildIndex(reinterpret_cast<Child*>(neighbor));

  for (Message *message = send_queue_.GetHead(); message; message = send_queue_.GetNext(message)) {
    if (message->GetDirectTransmission() == false && message->GetChildMask(child_index)) {
      neighbor->data_request = true;
      break;
    }
  }

  schedule_transmission_task_.Post();

exit:
  {}
}

}  // namespace Thread
