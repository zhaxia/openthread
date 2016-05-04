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

#include <openthread-core-config.h>
#include <openthread-types.h>
#include <common/tasklet.hpp>
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
    kReassemblyTimeout = OPENTHREAD_CONFIG_6LOWPAN_REASSEMBLY_TIMEOUT,
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

/**
 * This class implements mesh forwarding within Thread.
 *
 */
class MeshForwarder
{
public:
    /**
     * This constructor initializes the object.
     *
     * @param[in]  aThreadNetif  A reference to the Thread network interface.
     *
     */
    explicit MeshForwarder(ThreadNetif &aThreadNetif);

    /**
     * This method enables mesh forwarding and the IEEE 802.15.4 MAC layer.
     *
     * @retval kThreadError_None          Successfully enabled the mesh forwarder.
     * @retval kThreadError_InvalidState  The mesh forwarder was already enabled.
     *
     */
    ThreadError Start(void);

    /**
     * This method disables mesh forwarding and the IEEE 802.15.4 MAC layer.
     *
     * @retval kThreadError_None          Successfully disabled the mesh forwarder.
     * @retval kThreadError_InvalidState  The mesh forwarder was already disabled.
     *
     */
    ThreadError Stop(void);

    /**
     * This method submits a message to the mesh forwarder for forwarding.
     *
     * @param[in]  aMessage  A reference to the message.
     *
     * @retval kThreadError_None  Successfully enqueued the message.
     *
     */
    ThreadError SendMessage(Message &aMessage);

    /**
     * This method is called by the address resolver when an EID-to-RLOC mapping has been resolved.
     *
     * @param[in]  aEid  A reference to the EID that has been resolved.
     *
     */
    void HandleResolved(const Ip6::Address &aEid);

    /**
     * This method indicates whether or not rx-on-when-idle mode is enabled.
     *
     * @retval TRUE   The rx-on-when-idle mode is enabled.
     * @retval FALSE  The rx-on-when-idle-mode is disabled.
     *
     */
    bool GetRxOnWhenIdle(void);

    /**
     * This method sets the rx-on-when-idle mode
     *
     * @param[in]  aRxOnWhenIdle  TRUE to enable, FALSE otherwise.
     *
     */
    void SetRxOnWhenIdle(bool aRxOnWhenIdle);

    /**
     * This method sets the Data Poll period.
     *
     * @param[in]  aPeriod  The Data Poll period in milliseconds.
     *
     */
    void SetPollPeriod(uint32_t aPeriod);

private:
    enum
    {
        kStateUpdatePeriod = 1000,  ///< State update period in milliseconds.
    };

    ThreadError CheckReachability(uint8_t *aFrame, uint8_t aFrameLength,
                                  const Mac::Address &aMeshSource, const Mac::Address &aMeshDest);
    ThreadError GetMacDestinationAddress(const Ip6::Address &aIp6Addr, Mac::Address &aMacAddr);
    ThreadError GetMacSourceAddress(const Ip6::Address &aIp6Addr, Mac::Address &aMacAddr);
    Message *GetDirectTransmission(void);
    Message *GetIndirectTransmission(const Child &aChild);
    void HandleMesh(uint8_t *aFrame, uint8_t aPayloadLength, const ThreadMessageInfo &aMessageInfo);
    void HandleFragment(uint8_t *aFrame, uint8_t aPayloadLength,
                        const Mac::Address &aMacSource, const Mac::Address &aMacDest,
                        const ThreadMessageInfo &aMessageInfo);
    void HandleLowpanHC(uint8_t *aFrame, uint8_t aPayloadLength,
                        const Mac::Address &aMacSource, const Mac::Address &aMacDest,
                        const ThreadMessageInfo &aMessageInfo);
    void HandleDataRequest(const Mac::Address &aMacSource);
    void MoveToResolving(const Ip6::Address &aDestination);
    ThreadError SendPoll(Message &aMessage, Mac::Frame &aFrame);
    ThreadError SendMesh(Message &aMessage, Mac::Frame &aFrame);
    ThreadError SendFragment(Message &aMessage, Mac::Frame &aFrame);
    void UpdateFramePending(void);
    ThreadError UpdateIp6Route(Message &aMessage);
    ThreadError UpdateMeshRoute(Message &aMessage);

    static void HandleReceivedFrame(void *aContext, Mac::Frame &aFrame, ThreadError aError);
    void HandleReceivedFrame(Mac::Frame &aFrame, ThreadError aError);

    static ThreadError HandleFrameRequest(void *aContext, Mac::Frame &aFrame);
    ThreadError HandleFrameRequest(Mac::Frame &aFrame);

    static void HandleSentFrame(void *aContext, Mac::Frame &aFrame);
    void HandleSentFrame(Mac::Frame &aFrame);

    static void HandleReassemblyTimer(void *aContext);
    void HandleReassemblyTimer(void);
    static void HandlePollTimer(void *aContext);
    void HandlePollTimer(void);

    static void ScheduleTransmissionTask(void *aContext);
    void ScheduleTransmissionTask(void);

    Mac::Receiver mMacReceiver;
    Mac::Sender mMacSender;
    Timer mPollTimer;
    Timer mReassemblyTimer;

    MessageQueue mSendQueue;
    MessageQueue mReassemblyList;
    MessageQueue mResolvingQueue;
    uint16_t mFragTag;
    uint16_t mMessageNextOffset;
    uint32_t mPollPeriod;
    Message *mSendMessage;

    Mac::Address mMacSource;
    Mac::Address mMacDest;
    uint16_t mMeshSource;
    uint16_t mMeshDest;
    bool mAddMeshHeader;

    bool mSendBusy;

    Tasklet mScheduleTransmissionTask;
    bool mEnabled;

    Ip6::Netif &mNetif;
    AddressResolver &mAddressResolver;
    Lowpan::Lowpan &mLowpan;
    Mac::Mac &mMac;
    Mle::MleRouter &mMle;
    NetworkData::Leader &mNetworkData;
};

/**
 * @}
 *
 */

}  // namespace Thread

#endif  // MESH_FORWARDER_HPP_
