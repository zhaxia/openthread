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

/**
 * @file
 *   This file includes definitions for forwarding IPv6 datagrams across the Thread mesh.
 */

#ifndef MESH_FORWARDER_HPP_
#define MESH_FORWARDER_HPP_

#include <common/tasklet.hpp>
#include <common/thread_error.hpp>
#include <mac/mac.hpp>
#include <net/ip6.hpp>
#include <net/netif.hpp>
#include <thread/address_resolver.hpp>
#include <thread/lowpan.hpp>
#include <thread/network_data_leader.hpp>
#include <thread/topology.hpp>

namespace Thread {

enum
{
    kReassemblyTimeout = 5,  // seconds
};

class MleRouter;
struct ThreadMessageInfo;

/**
 * @addtogroup core-mesh-forwarding
 *
 * @brief
 *   This module includes definitions for mesh forwarding within Thread.
 *
 * @{
 */

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
    ThreadError CheckReachability(uint8_t *frame, uint8_t frameLength,
                                  const Mac::Address &meshsrc, const Mac::Address &meshdst);
    ThreadError GetMacDestinationAddress(const Ip6Address &ipaddr, Mac::Address &macaddr);
    ThreadError GetMacSourceAddress(const Ip6Address &ipaddr, Mac::Address &macaddr);
    Message *GetDirectTransmission();
    Message *GetIndirectTransmission(const Child &child);
    void HandleMesh(uint8_t *frame, uint8_t payloadLength,
                    const Mac::Address &macsrc, const Mac::Address &macdst, const ThreadMessageInfo &messageInfo);
    void HandleFragment(uint8_t *frame, uint8_t payloadLength, const Mac::Address &macsrc, const Mac::Address &macdst,
                        const ThreadMessageInfo &messageInfo);
    void HandleLowpanHC(uint8_t *frame, uint8_t payloadLength, const Mac::Address &macsrc, const Mac::Address &macdst,
                        const ThreadMessageInfo &messageInfo);
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

    Mac::Receiver mMacReceiver;
    Mac::Sender mMacSender;
    Timer mPollTimer;
    Timer mReassemblyTimer;

    MessageQueue mSendQueue;
    MessageQueue mReassemblyList;
    MessageQueue mResolvingQueue;
    uint16_t mFragTag;
    uint16_t mMessageNextOffset;
    uint32_t mPollPeriod = 0;
    Message *mSendMessage = NULL;

    Mac::Address mMacsrc;
    Mac::Address mMacdst;
    uint16_t mMeshsrc;
    uint16_t mMeshdst;
    bool mAddMeshHeader;

    bool mSendBusy = false;

    Tasklet mScheduleTransmissionTask;
    bool mEnabled = false;

    Lowpan *mLowpan;
    Netif *mNetif;
    AddressResolver *mAddressResolver;
    Mac::Mac *mMac;
    NetworkData::Leader *mNetworkData;
    Mle::MleRouter *mMle;
};

/**
 * @}
 *
 */

}  // namespace Thread

#endif  // MESH_FORWARDER_HPP_
