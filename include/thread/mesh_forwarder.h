/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef MESH_FORWARDER_H_
#define MESH_FORWARDER_H_

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

enum
{
    kReassemblyTimeout = 5,  // seconds
};

class MleRouter;
struct ThreadMessageInfo;

class MeshForwarder
{
public:
    explicit MeshForwarder(ThreadNetif &netif);
    ThreadError Start();
    ThreadError Stop();

    ThreadError SendMessage(Message &message);

    const Mac::Address64 *GetAddress64() const;

    Mac::Address16 GetAddress16() const;
    ThreadError SetAddress16(Mac::Address16 address16);
    void HandleResolved(const Ip6Address &eid);

    bool GetRxOnWhenIdle();
    ThreadError SetRxOnWhenIdle(bool rx_on_when_idle);
    ThreadError SetPollPeriod(uint32_t period);

private:
    ThreadError CheckReachability(uint8_t *frame, uint8_t frame_length,
                                  const Mac::Address &meshsrc, const Mac::Address &meshdst);
    ThreadError GetMacDestinationAddress(const Ip6Address &ipaddr, Mac::Address &macaddr);
    ThreadError GetMacSourceAddress(const Ip6Address &ipaddr, Mac::Address &macaddr);
    Message *GetDirectTransmission();
    Message *GetIndirectTransmission(const Child &child);
    void HandleMesh(uint8_t *frame, uint8_t payload_length,
                    const Mac::Address &macsrc, const Mac::Address &macdst, const ThreadMessageInfo &message_info);
    void HandleFragment(uint8_t *frame, uint8_t payload_length, const Mac::Address &macsrc, const Mac::Address &macdst,
                        const ThreadMessageInfo &message_info);
    void HandleLowpanHC(uint8_t *frame, uint8_t payload_length, const Mac::Address &macsrc, const Mac::Address &macdst,
                        const ThreadMessageInfo &message_info);
    void HandleDataRequest(const Mac::Address &macsrc);
    void MoveToResolving(const Ip6Address &destination);
    ThreadError SendPoll(Message &message, Mac::Frame &frame);
    ThreadError SendMesh(Message &message, Mac::Frame &frame);
    ThreadError SendFragment(Message &message, Mac::Frame &frame);
    void UpdateFramePending();
    ThreadError UpdateIp6Route(Message &message);
    ThreadError UpdateMeshRoute(Message &message);

    static void HandleReceivedFrame(void *context, Mac::Frame &frame, ThreadError error);
    void HandleReceivedFrame(Mac::Frame &frame, ThreadError error);

    static ThreadError HandleFrameRequest(void *context, Mac::Frame &frame);
    ThreadError HandleFrameRequest(Mac::Frame &frame);

    static void HandleSentFrame(void *context, Mac::Frame &frame);
    void HandleSentFrame(Mac::Frame &frame);

    static void HandleReassemblyTimer(void *context);
    void HandleReassemblyTimer();
    static void HandlePollTimer(void *context);
    void HandlePollTimer();

    static void ScheduleTransmissionTask(void *context);
    void ScheduleTransmissionTask();

    Mac::Receiver m_mac_receiver;
    Mac::Sender m_mac_sender;
    Timer m_poll_timer;
    Timer m_reassembly_timer;

    MessageQueue m_send_queue;
    MessageQueue m_reassembly_list;
    MessageQueue m_resolving_queue;
    uint16_t m_frag_tag;
    uint16_t m_message_next_offset;
    uint32_t m_poll_period = 0;
    Message *m_send_message = NULL;

    Mac::Address m_macsrc;
    Mac::Address m_macdst;
    uint16_t m_meshsrc;
    uint16_t m_meshdst;
    bool m_add_mesh_header;

    bool m_send_busy = false;


    Tasklet m_schedule_transmission_task;
    bool m_enabled = false;

    Lowpan *m_lowpan;
    Netif *m_netif;
    AddressResolver *m_address_resolver;
    Mac::Mac *m_mac;
    NetworkData::Leader *m_network_data;
    Mle::MleRouter *m_mle;
};

}  // namespace Thread

#endif  // MESH_FORWARDER_H_
