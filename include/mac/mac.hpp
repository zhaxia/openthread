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
 *   This file contains definitions for the IEEE 802.15.4 MAC.
 */

#ifndef MAC_HPP_
#define MAC_HPP_

#include <common/tasklet.hpp>
#include <common/timer.hpp>
#include <crypto/aes_ecb.hpp>
#include <mac/mac_frame.hpp>
#include <mac/mac_whitelist.hpp>
#include <platform/radio.h>
#include <thread/key_manager.hpp>
#include <thread/topology.hpp>

namespace Thread {

namespace Mle { class MleRouter; }

namespace Mac {

enum
{
    kMacAckTimeout = 16,  // milliseconds
    kDataTimeout = 100,  // milliseconds

    kMacScanChannelMaskAllChannels = 0xffff,
    kMacScanDefaultInterval = 128  // milliseconds
};

struct ActiveScanResult
{
    uint8_t mNetworkName[16];
    uint8_t mExtPanid[8];
    uint8_t mExtAddr[8];
    uint16_t mPanid;
    uint8_t mChannel;
    int8_t mRssi;
};

class Receiver
{
    friend class Mac;

public:
    typedef void (*ReceiveFrameHandler)(void *context, Frame &frame, ThreadError error);
    Receiver(ReceiveFrameHandler receiveFrameHandler, void *context) {
        mReceiveFrameHandler = receiveFrameHandler;
        mContext = context;
        mNext = NULL;
    }

private:
    void HandleReceivedFrame(Frame &frame, ThreadError error) { mReceiveFrameHandler(mContext, frame, error); }

    ReceiveFrameHandler mReceiveFrameHandler;
    void *mContext;
    Receiver *mNext;
};

class Sender
{
    friend class Mac;

public:
    typedef ThreadError(*FrameRequestHandler)(void *context, Frame &frame);
    typedef void (*SentFrameHandler)(void *context, Frame &frame);
    Sender(FrameRequestHandler frameRequestHandler, SentFrameHandler sentFrameHandler, void *context) {
        mFrameRequestHandler = frameRequestHandler;
        mSentFrameHandler = sentFrameHandler;
        mContext = context;
        mNext = NULL;
    }

private:
    ThreadError HandleFrameRequest(Frame &frame) { return mFrameRequestHandler(mContext, frame); }
    void HandleSentFrame(Frame &frame) { mSentFrameHandler(mContext, frame); }

    FrameRequestHandler mFrameRequestHandler;
    SentFrameHandler mSentFrameHandler;
    void *mContext;
    Sender *mNext;
};

class Mac
{
public:
    explicit Mac(ThreadNetif *netif);
    ThreadError Init();
    ThreadError Start();
    ThreadError Stop();

    typedef void (*ActiveScanHandler)(void *context, ActiveScanResult *result);
    ThreadError ActiveScan(uint16_t intervalPerChannel, uint16_t channelMask,
                           ActiveScanHandler handler, void *context);

    bool GetRxOnWhenIdle() const;
    ThreadError SetRxOnWhenIdle(bool rxOnWhenIdle);

    ThreadError RegisterReceiver(Receiver &receiver);
    ThreadError SendFrameRequest(Sender &sender);

    const Address64 *GetAddress64() const;
    Address16 GetAddress16() const;
    ThreadError SetAddress16(Address16 address16);

    uint8_t GetChannel() const;
    ThreadError SetChannel(uint8_t channel);

    const char *GetNetworkName() const;
    ThreadError SetNetworkName(const char *name);

    uint16_t GetPanId() const;
    ThreadError SetPanId(uint16_t panid);

    const uint8_t *GetExtendedPanId() const;
    ThreadError SetExtendedPanId(const uint8_t *xpanid);

    void HandleReceiveDone(RadioPacket *packet, ThreadError error);
    void HandleTransmitDone(RadioPacket *packet, bool rxPending, ThreadError error);

    Whitelist *GetWhitelist();

    static void ReceiveDoneTask(void *context);
    static void TransmitDoneTask(void *context);

private:
    void GenerateNonce(const Address64 &address, uint32_t frameCounter, uint8_t securityLevel, uint8_t *nonce);
    void NextOperation();
    void ProcessTransmitSecurity();
    ThreadError ProcessReceiveSecurity(const Address &srcaddr, Neighbor &neighbor);
    void ScheduleNextTransmission();
    void SentFrame(bool acked);
    void SendBeaconRequest(Frame *frame);
    void SendBeacon(Frame *frame);
    void StartBackoff();
    void HandleBeaconFrame();
    ThreadError HandleMacCommand();

    static void HandleAckTimer(void *context);
    void HandleAckTimer();
    static void HandleBackoffTimer(void *context);
    void HandleBackoffTimer();
    static void HandleReceiveTimer(void *context);
    void HandleReceiveTimer();

    void ReceiveDoneTask();
    void TransmitDoneTask();

    Timer mAckTimer;
    Timer mBackoffTimer;
    Timer mReceiveTimer;

    KeyManager *mKeyManager = NULL;

    Address64 mAddress64;
    Address16 mAddress16 = kShortAddrInvalid;
    uint16_t mPanid = kShortAddrInvalid;
    uint8_t mExtendedPanid[8];
    char mNetworkName[16];
    uint8_t mChannel = 12;

    Frame mSendFrame;
    Frame mReceiveFrame;
    Sender *mSendHead = NULL, *mSendTail = NULL;
    Receiver *mReceiveHead = NULL, *mReceiveTail = NULL;
    Mle::MleRouter *mMle;

    enum
    {
        kStateDisabled = 0,
        kStateIdle,
        kStateActiveScan,
        kStateTransmitBeacon,
        kStateTransmitData,
    };
    uint8_t mState = kStateDisabled;

    uint8_t mBeaconSequence;
    uint8_t mDataSequence;
    bool mRxOnWhenIdle = true;
    uint8_t mAttempts = 0;
    bool mTransmitBeacon = false;

    bool mActiveScanRequest = false;
    uint8_t mScanChannel = 11;
    uint16_t mScanChannelMask = 0xff;
    uint16_t mScanIntervalPerChannel = 0;
    ActiveScanHandler mActiveScanHandler = NULL;
    void *mActiveScanContext = NULL;

    Whitelist mWhitelist;
};

}  // namespace Mac
}  // namespace Thread

#endif  // MAC_MAC_HPP_
