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
 *   This file includes definitions for the IEEE 802.15.4 MAC.
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

/**
 * @addtogroup core-mac
 *
 * @brief
 *   This module includes definitions for the IEEE 802.15.4 MAC
 *
 * @{
 *
 */

namespace Mac {

enum
{
    kMacAckTimeout = 16,  // milliseconds
    kDataTimeout = 100,  // milliseconds

    kMacScanChannelMaskAllChannels = 0xffff,
    kMacScanDefaultInterval = 128,  // milliseconds

    kNetworkNameSize = 16,   ///< Size of Thread Network Name in bytes.
    kExtPanIdSize = 8,           ///< Size of Thread Extended PAN ID.
};

/**
 * This structure represents an Active Scan result.
 *
 */
struct ActiveScanResult
{
    uint8_t  mNetworkName[kNetworkNameSize];   ///<  The Thread Network Name.
    uint8_t  mExtPanid[kExtPanIdSize];             ///<  The Thread Extended PAN ID.
    uint8_t  mExtAddr[ExtAddress::kLength];    ///<  The IEEE 802.15.4 Extended Address.
    uint16_t mPanId;                           ///<  The IEEE 802.15.4 PAN ID.
    uint8_t  mChannel;                         ///<  The IEEE 802.15.4 Channel.
    int8_t   mRssi;                            ///<  The RSSI in dBm.
};

/**
 * This class implements a MAC receiver client.
 *
 */
class Receiver
{
    friend class Mac;

public:
    /**
     * This function pointer is called when a MAC frame is received.
     *
     * @param[in]  aContext  A pointer to arbitrary context information.
     * @param[in]  aFrame    A reference to the MAC frame.
     * @param[in]  aError    Any errors that occured during reception.
     *
     */
    typedef void (*ReceiveFrameHandler)(void *aContext, Frame &aFrame, ThreadError aError);

    /**
     * This constructor creates a MAC receiver client.
     *
     * @param[in]  aReceiveFrameHandler  A pointer to a function that is called on MAC frame reception.
     * @param[in]  aContext              A pointer to arbitrary context information.
     *
     */
    Receiver(ReceiveFrameHandler aReceiveFrameHandler, void *aContext) {
        mReceiveFrameHandler = aReceiveFrameHandler;
        mContext = aContext;
        mNext = NULL;
    }

private:
    void HandleReceivedFrame(Frame &frame, ThreadError error) { mReceiveFrameHandler(mContext, frame, error); }

    ReceiveFrameHandler mReceiveFrameHandler;
    void *mContext;
    Receiver *mNext;
};

/**
 * This class implements a MAC sender client.
 *
 */
class Sender
{
    friend class Mac;

public:
    /**
     * This function pointer is called when the MAC is about to transmit the frame.
     *
     * @param[in]  aContext  A pointer to arbitrary context information.
     * @param[in]  aFrame    A reference to the MAC frame buffer.
     *
     */
    typedef ThreadError(*FrameRequestHandler)(void *aContext, Frame &aFrame);

    /**
     * This function pointer is called when the MAC is done sending the frame.
     *
     * @param[in]  aContext  A pointer to arbitrary context information.
     * @param[in]  aFrame    A reference to the MAC frame buffer that was sent.
     *
     */
    typedef void (*SentFrameHandler)(void *aContext, Frame &aFrame);

    /**
     * This constructor creates a MAC sender client.
     *
     * @param[in]  aFrameRequestHandler  A pointer to a function that is called when about to send a MAC frame.
     * @param[in]  aSentFrameHandler     A pointer to a function that is called when done sending the frame.
     * @param[in]  aContext              A pointer to arbitrary context information.
     *
     */
    Sender(FrameRequestHandler aFrameRequestHandler, SentFrameHandler aSentFrameHandler, void *aContext) {
        mFrameRequestHandler = aFrameRequestHandler;
        mSentFrameHandler = aSentFrameHandler;
        mContext = aContext;
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

/**
 * This class implements the IEEE 802.15.4 MAC.
 *
 */
class Mac
{
public:
    /**
     * This constructor creates the MAC object.
     *
     */
    Mac(void);

    /**
     * This method initializes the MAC.
     *
     * @param[in]  aNetif  A reference to the network interface using this MAC.
     *
     */
    ThreadError Init(ThreadNetif &aNetif);

    /**
     * This method starts the MAC.
     *
     * @retval kThreadError_None  Successfully started the MAC.
     * @retval kThreadError_Busy  The MAC could not be started.
     *
     */
    ThreadError Start(void);

    /**
     * This method stops the MAC.
     *
     * @retval kThreadError_None  Successfully stopped the MAC.
     * @retval kThreadError_Busy  The MAC could not be stopped.
     *
     */
    ThreadError Stop(void);

    /**
     * This function pointer is called on receiving an IEEE 802.15.4 Beacon during an Active Scan.
     *
     * @param[in]  aContext  A pointer to arbitrary context information.
     * @param[in]  aResult   A reference to the Active Scan result.
     *
     */
    typedef void (*ActiveScanHandler)(void *aContext, ActiveScanResult *aResult);

    /**
     * This method starts an IEEE 802.15.4 Active Scan.
     *
     * @param[in]  aIntervalPerChannel  The time in milliseconds to spend scanning each channel.
     * @param[in]  aChannelMask         A bit vector indicating which channels to scan.
     * @param[in]  aHandler             A pointer to a function that is called on receiving an IEEE 802.15.4 Beacon.
     * @param[in]  aContext             A pointer to arbitrary context information.
     *
     */
    ThreadError ActiveScan(uint16_t aIntervalPerChannel, uint16_t aChannelMask,
                           ActiveScanHandler aHandler, void *aContext);

    /**
     * This method indicates whether or not rx-on-when-idle is enabled.
     *
     * @retval TRUE   If rx-on-when-idle is enabled.
     * @retval FALSE  If rx-on-when-idle is not enabled.
     */
    bool GetRxOnWhenIdle(void) const;

    /**
     * This method sets the rx-on-when-idle mode.
     *
     * @param[in]  aRxOnWhenIdel  The rx-on-when-idle mode.
     *
     */
    void SetRxOnWhenIdle(bool aRxOnWhenIdle);

    /**
     * This method registers a new MAC receiver client.
     *
     * @param[in]  aReceiver  A reference to the MAC receiver client.
     *
     * @retval kThreadError_None  Successfully registered the receiver.
     * @retval kThreadError_Busy  The receiver was already registered.
     *
     */
    ThreadError RegisterReceiver(Receiver &receiver);

    /**
     * This method registers a new MAC sender client.
     *
     * @param[in]  aSender  A reference to the MAC sender client.
     *
     * @retval kThreadError_None  Successfully registered the sender.
     * @retval kThreadError_Busy  The sender was already registered.
     *
     */
    ThreadError SendFrameRequest(Sender &aSender);

    /**
     * This method returns a pointer to the IEEE 802.15.4 Extended Address.
     *
     * @returns A pointer to the IEEE 802.15.4 Extended Address.
     *
     */
    const ExtAddress *GetExtAddress(void) const;

    /**
     * This method returns the IEEE 802.15.4 Short Address.
     *
     * @returns The IEEE 802.15.4 Short Address.
     *
     */
    ShortAddress GetShortAddress(void) const;

    /**
     * This method sets the IEEE 802.15.4 Short Address.
     *
     * @param[in]  aShortAddress  The IEEE 802.15.4 Short Address.
     *
     * @retval kThreadError_None  Successfully set the IEEE 802.15.4 Short Address.
     *
     */
    ThreadError SetShortAddress(ShortAddress aShortAddress);

    /**
     * This method returns the IEEE 802.15.4 Channel.
     *
     * @returns The IEEE 802.15.4 Channel.
     *
     */
    uint8_t GetChannel(void) const;

    /**
     * This method sets the IEEE 802.15.4 Channel.
     *
     * @param[in]  aChannel  The IEEE 802.15.4 Channel.
     *
     * @retval kThreadError_None  Successfully set the IEEE 802.15.4 Channel.
     *
     */
    ThreadError SetChannel(uint8_t aChannel);

    /**
     * This method returns the IEEE 802.15.4 Network Name.
     *
     * @returns A pointer to the IEEE 802.15.4 Network Name.
     *
     */
    const char *GetNetworkName(void) const;

    /**
     * This method sets the IEEE 802.15.4 Network Name.
     *
     * @param[in]  aNetworkName  A pointer to the IEEE 802.15.4 Network Name.
     *
     * @retval kThreadError_None  Successfully set the IEEE 802.15.4 Network Name.
     *
     */
    ThreadError SetNetworkName(const char *aNetworkName);

    /**
     * This method returns the IEEE 802.15.4 PAN ID.
     *
     * @returns The IEEE 802.15.4 PAN ID.
     *
     */
    uint16_t GetPanId(void) const;

    /**
     * This method sets the IEEE 802.15.4 PAN ID.
     *
     * @param[in]  aPanId  The IEEE 802.15.4 PAN ID.
     *
     * @retval kThreadError_None  Successfully set the IEEE 802.15.4 PAN ID.
     *
     */
    ThreadError SetPanId(uint16_t aPanId);

    /**
     * This method returns the IEEE 802.15.4 Extended PAN ID.
     *
     * @returns A pointer to the IEEE 802.15.4 Extended PAN ID.
     *
     */
    const uint8_t *GetExtendedPanId(void) const;

    /**
     * This method sets the IEEE 802.15.4 Extended PAN ID.
     *
     * @param[in]  aExtPanId  The IEEE 802.15.4 Extended PAN ID.
     *
     * @retval kThreadError_None  Successfully set the IEEE 802.15.4 Extended PAN ID.
     *
     */
    ThreadError SetExtendedPanId(const uint8_t *aExtPanId);

    /**
     * This method returns the MAC whitelist filter.
     *
     * @returns A reference to the MAC whitelist filter.
     *
     */
    Whitelist &GetWhitelist(void);

    static void ReceiveDoneTask(void *context);
    static void TransmitDoneTask(void *context);

private:
    void GenerateNonce(const ExtAddress &address, uint32_t frameCounter, uint8_t securityLevel, uint8_t *nonce);
    void NextOperation(void);
    void ProcessTransmitSecurity(void);
    ThreadError ProcessReceiveSecurity(const Address &srcaddr, Neighbor &neighbor);
    void ScheduleNextTransmission(void);
    void SentFrame(bool acked);
    void SendBeaconRequest(Frame &frame);
    void SendBeacon(Frame &frame);
    void StartBackoff(void);
    void HandleBeaconFrame(void);
    ThreadError HandleMacCommand(void);

    static void HandleAckTimer(void *context);
    void HandleAckTimer(void);
    static void HandleBackoffTimer(void *context);
    void HandleBackoffTimer(void);
    static void HandleReceiveTimer(void *context);
    void HandleReceiveTimer(void);

    void ReceiveDoneTask(void);
    void TransmitDoneTask(void);

    Timer mAckTimer;
    Timer mBackoffTimer;
    Timer mReceiveTimer;

    KeyManager *mKeyManager;

    ExtAddress mExtAddress;
    ShortAddress mShortAddress;
    uint16_t mPanId;
    uint8_t mExtendedPanid[kExtPanIdSize];
    char mNetworkName[kNetworkNameSize];
    uint8_t mChannel;

    Frame mSendFrame;
    Frame mReceiveFrame;
    Sender *mSendHead, *mSendTail;
    Receiver *mReceiveHead, *mReceiveTail;
    Mle::MleRouter *mMle;

    enum
    {
        kStateDisabled = 0,
        kStateIdle,
        kStateActiveScan,
        kStateTransmitBeacon,
        kStateTransmitData,
    };
    uint8_t mState;

    uint8_t mBeaconSequence;
    uint8_t mDataSequence;
    bool mRxOnWhenIdle;
    uint8_t mAttempts;
    bool mTransmitBeacon;

    bool mActiveScanRequest;
    uint8_t mScanChannel;
    uint16_t mScanChannelMask;
    uint16_t mScanIntervalPerChannel;
    ActiveScanHandler mActiveScanHandler;
    void *mActiveScanContext;

    Whitelist mWhitelist;
};

/**
 * @}
 *
 */

}  // namespace Mac
}  // namespace Thread

#endif  // MAC_HPP_
