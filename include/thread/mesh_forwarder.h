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

#ifndef THREAD_MESH_FORWARDER_H_
#define THREAD_MESH_FORWARDER_H_

#include <common/tasklet.h>
#include <common/thread_error.h>
#include <mac/mac.h>
#include <net/ip6.h>
#include <net/netif.h>
#include <thread/address_resolver.h>
#include <thread/lowpan.h>
#include <thread/network_data_leader.h>
#include <thread/topology.h>
#include <pthread.h>

namespace Thread {

enum {
  kReassemblyTimeout = 5,  // seconds
};

class MleRouter;
struct ThreadMessageInfo;

class MeshForwarder {
 public:
  MeshForwarder(AddressResolver *address_resolver, Mac *mac, MleRouter *mle, Netif *netif,
                NetworkDataLeader *network_data);
  ThreadError Start();
  ThreadError Stop();

  ThreadError SendMessage(Message *message);

  const MacAddr64 *GetAddress64() const;

  MacAddr16 GetAddress16() const;
  ThreadError SetAddress16(MacAddr16 address16);
  void HandleResolved(const Ip6Address *eid);

  bool GetRxOnWhenIdle();
  ThreadError SetRxOnWhenIdle(bool rx_on_when_idle);
  ThreadError SetPollPeriod(uint32_t period);

 private:
  void MoveToResolving(const Ip6Address *destination);
  ThreadError UpdateIp6Route(Message *message);
  ThreadError UpdateMeshRoute(Message *message);
  ThreadError SendPoll(Message *message, MacFrame *frame);
  ThreadError SendMesh(Message *message, MacFrame *frame);
  ThreadError SendFragment(Message *message, MacFrame *frame);
  ThreadError CheckReachability(uint8_t *frame, uint8_t frame_length, MacAddress *meshsrc, MacAddress *meshdst);
  void UpdateFramePending();
  void HandleMesh(uint8_t *frame, uint8_t payload_length, MacAddress *macsrc, MacAddress *macdst,
                  ThreadMessageInfo *message_info);
  void HandleFragment(uint8_t *frame, uint8_t payload_length, MacAddress *macsrc, MacAddress *macdst,
                      ThreadMessageInfo *message_info);
  void HandleLowpanHC(uint8_t *frame, uint8_t payload_length, MacAddress *macsrc, MacAddress *macdst,
                      ThreadMessageInfo *message_info);
  void HandleDataRequest(MacAddress *macsrc);

  static void HandleReceivedFrame(void *context, MacFrame *frame, ThreadError error);
  void HandleReceivedFrame(MacFrame *frame, ThreadError error);

  static ThreadError HandleFrameRequest(void *context, MacFrame *frame);
  ThreadError HandleFrameRequest(MacFrame *frame);

  static void HandleSentFrame(void *context, MacFrame *frame);
  void HandleSentFrame(MacFrame *frame);

  static void HandleReassemblyTimer(void *context);
  void HandleReassemblyTimer();
  static void HandlePollTimer(void *context);
  void HandlePollTimer();

  ThreadError GetMacDestinationAddress(const Ip6Address *ipaddr, MacAddress *macaddr);
  ThreadError GetMacSourceAddress(const Ip6Address *ipaddr, MacAddress *macaddr);

  Lowpan lowpan_;
  Mac::Receiver mac_receiver_;
  Mac::Sender mac_sender_;
  Timer poll_timer_;
  Timer reassembly_timer_;

  Netif *netif_ = NULL;
  AddressResolver *address_resolver_;
  Mac *mac_;
  NetworkDataLeader *network_data_;
  MleRouter *mle_;
  bool enabled_ = false;

  MessageQueue send_queue_;
  MessageQueue reassembly_list_;
  MessageQueue resolving_queue_;
  uint16_t frag_tag_;
  uint16_t message_next_offset_;
  uint32_t poll_period_ = 0;
  Message *send_message_ = NULL;

  Message *GetDirectTransmission();
  Message *GetIndirectTransmission(Child *child);
  bool send_busy_ = false;

  static void ScheduleTransmissionTask(void *context);
  void ScheduleTransmissionTask();
  Tasklet schedule_transmission_task_;
};

}  // namespace Thread

#endif  // THREAD_MESH_FORWARDER_H_
