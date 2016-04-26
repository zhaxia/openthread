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
 *   This file implements the subset of IEEE 802.15.4 primitives required for Thread.
 */

#include <common/code_utils.hpp>
#include <common/thread_error.hpp>
#include <crypto/aes_ccm.hpp>
#include <mac/mac.hpp>
#include <mac/mac_frame.hpp>
#include <platform/random.h>
#include <thread/mle_router.hpp>
#include <thread/thread_netif.hpp>

namespace Thread {
namespace Mac {

static const uint8_t sExtendedPanidInit[] = {0xde, 0xad, 0x00, 0xbe, 0xef, 0x00, 0xca, 0xfe};
static const char sNetworkNameInit[] = "OpenThread";
static Mac *sMac;

static Tasklet sReceiveDoneTask(&Mac::ReceiveDoneTask, NULL);
static Tasklet sTransmitDoneTask(&Mac::TransmitDoneTask, NULL);

Mac::Mac(ThreadNetif *netif):
    mAckTimer(&HandleAckTimer, this),
    mBackoffTimer(&HandleBackoffTimer, this),
    mReceiveTimer(&HandleReceiveTimer, this)
{
    mKeyManager = netif->GetKeyManager();
    mMle = netif->GetMle();

    memcpy(mExtendedPanid, sExtendedPanidInit, sizeof(mExtendedPanid));
    memcpy(mNetworkName, sNetworkNameInit, sizeof(mNetworkName));

    sMac = this;
}

ThreadError Mac::Init()
{
    for (size_t i = 0; i < sizeof(mAddress64); i++)
    {
        mAddress64.mBytes[i] = ot_random_get();
    }

    mBeaconSequence = ot_random_get();
    mDataSequence = ot_random_get();

    ot_radio_init();

    return kThreadError_None;
}

ThreadError Mac::Start()
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mState == kStateDisabled, ;);

    SuccessOrExit(error = ot_radio_enable());

    SetExtendedPanId(mExtendedPanid);
    ot_radio_set_pan_id(mPanid);
    ot_radio_set_short_address(mAddress16);
    {
        uint8_t buf[8];

        for (size_t i = 0; i < sizeof(buf); i++)
        {
            buf[i] = mAddress64.mBytes[7 - i];
        }

        ot_radio_set_extended_address(buf);
    }
    mState = kStateIdle;
    NextOperation();

exit:
    return error;
}

ThreadError Mac::Stop()
{
    ThreadError error = kThreadError_None;

    SuccessOrExit(error = ot_radio_disable());
    mAckTimer.Stop();
    mBackoffTimer.Stop();
    mState = kStateDisabled;

    while (mSendHead != NULL)
    {
        Sender *cur;
        cur = mSendHead;
        mSendHead = mSendHead->mNext;
        cur->mNext = NULL;
    }

    mSendTail = NULL;

    while (mReceiveHead != NULL)
    {
        Receiver *cur;
        cur = mReceiveHead;
        mReceiveHead = mReceiveHead->mNext;
        cur->mNext = NULL;
    }

    mReceiveTail = NULL;

exit:
    return error;
}

ThreadError Mac::ActiveScan(uint16_t intervalPerChannel, uint16_t channelMask,
                            ActiveScanHandler handler, void *context)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(mState != kStateDisabled && mState != kStateActiveScan && mActiveScanRequest == false,
                 error = kThreadError_Busy);

    mActiveScanHandler = handler;
    mActiveScanContext = context;
    mScanChannelMask = (channelMask == 0) ? kMacScanChannelMaskAllChannels : channelMask;
    mScanIntervalPerChannel = (intervalPerChannel == 0) ? kMacScanDefaultInterval : intervalPerChannel;

    mScanChannel = 11;

    while ((mScanChannelMask & 1) == 0)
    {
        mScanChannelMask >>= 1;
        mScanChannel++;
    }

    if (mState == kStateIdle)
    {
        mState = kStateActiveScan;
        mBackoffTimer.Start(16);
    }
    else
    {
        mActiveScanRequest = true;
    }

exit:
    return error;
}

ThreadError Mac::RegisterReceiver(Receiver &receiver)
{
    assert(mReceiveTail != &receiver && receiver.mNext == NULL);

    if (mReceiveTail == NULL)
    {
        mReceiveHead = &receiver;
        mReceiveTail = &receiver;
    }
    else
    {
        mReceiveTail->mNext = &receiver;
        mReceiveTail = &receiver;
    }

    return kThreadError_None;
}

bool Mac::GetRxOnWhenIdle() const
{
    return mRxOnWhenIdle;
}

ThreadError Mac::SetRxOnWhenIdle(bool rxOnWhenIdle)
{
    mRxOnWhenIdle = rxOnWhenIdle;
    return kThreadError_None;
}

const Address64 *Mac::GetAddress64() const
{
    return &mAddress64;
}

Address16 Mac::GetAddress16() const
{
    return mAddress16;
}

ThreadError Mac::SetAddress16(Address16 address16)
{
    mAddress16 = address16;
    return ot_radio_set_short_address(address16);
}

uint8_t Mac::GetChannel() const
{
    return mChannel;
}

ThreadError Mac::SetChannel(uint8_t channel)
{
    mChannel = channel;
    return kThreadError_None;
}

const char *Mac::GetNetworkName() const
{
    return mNetworkName;
}

ThreadError Mac::SetNetworkName(const char *name)
{
    strncpy(mNetworkName, name, sizeof(mNetworkName));
    return kThreadError_None;
}

uint16_t Mac::GetPanId() const
{
    return mPanid;
}

ThreadError Mac::SetPanId(uint16_t panid)
{
    mPanid = panid;
    return ot_radio_set_pan_id(mPanid);
}

const uint8_t *Mac::GetExtendedPanId() const
{
    return mExtendedPanid;
}

ThreadError Mac::SetExtendedPanId(const uint8_t *xpanid)
{
    memcpy(mExtendedPanid, xpanid, sizeof(mExtendedPanid));
    mMle->SetMeshLocalPrefix(mExtendedPanid);
    return kThreadError_None;
}

ThreadError Mac::SendFrameRequest(Sender &sender)
{
    ThreadError error = kThreadError_None;
    uint32_t backoff;

    VerifyOrExit(mState != kStateDisabled &&
                 mSendTail != &sender && sender.mNext == NULL,
                 error = kThreadError_Busy);

    if (mSendHead == NULL)
    {
        mSendHead = &sender;
        mSendTail = &sender;
    }
    else
    {
        mSendTail->mNext = &sender;
        mSendTail = &sender;
    }

    if (mState == kStateIdle)
    {
        mState = kStateTransmitData;
        backoff = (ot_random_get() % 32) + 1;
        mBackoffTimer.Start(backoff);
    }

exit:
    return error;
}

void Mac::NextOperation()
{
    switch (mState)
    {
    case kStateDisabled:
        break;

    case kStateActiveScan:
        mReceiveFrame.SetChannel(mScanChannel);
        ot_radio_receive(&mReceiveFrame);
        break;

    default:
        if (mRxOnWhenIdle || mReceiveTimer.IsRunning())
        {
            mReceiveFrame.SetChannel(mChannel);
            ot_radio_receive(&mReceiveFrame);
        }
        else
        {
            ot_radio_sleep();
        }

        break;
    }
}

void Mac::ScheduleNextTransmission()
{
    if (mActiveScanRequest)
    {
        mActiveScanRequest = false;
        mState = kStateActiveScan;
        mBackoffTimer.Start(16);
    }
    else if (mTransmitBeacon)
    {
        mTransmitBeacon = false;
        mState = kStateTransmitBeacon;
        mBackoffTimer.Start(16);
    }
    else if (mSendHead != NULL)
    {
        mState = kStateTransmitData;
        mBackoffTimer.Start(16);
    }
    else
    {
        mState = kStateIdle;
    }
}

void Mac::GenerateNonce(const Address64 &address, uint32_t frameCounter, uint8_t securityLevel, uint8_t *nonce)
{
    // source address
    for (int i = 0; i < 8; i++)
    {
        nonce[i] = address.mBytes[i];
    }

    nonce += 8;

    // frame counter
    nonce[0] = frameCounter >> 24;
    nonce[1] = frameCounter >> 16;
    nonce[2] = frameCounter >> 8;
    nonce[3] = frameCounter >> 0;
    nonce += 4;

    // security level
    nonce[0] = securityLevel;
}

void Mac::SendBeaconRequest(Frame *frame)
{
    // initialize MAC header
    uint16_t fcf = Frame::kFcfFrameMacCmd | Frame::kFcfDstAddrShort | Frame::kFcfSrcAddrNone;
    frame->InitMacHeader(fcf, Frame::kSecNone);
    frame->SetDstPanId(kShortAddrBroadcast);
    frame->SetDstAddr(kShortAddrBroadcast);
    frame->SetCommandId(Frame::kMacCmdBeaconRequest);

    dprintf("Sent Beacon Request\n");
}

void Mac::SendBeacon(Frame *frame)
{
    uint8_t *payload;
    uint16_t fcf;

    // initialize MAC header
    fcf = Frame::kFcfFrameBeacon | Frame::kFcfDstAddrNone | Frame::kFcfSrcAddrExt;
    frame->InitMacHeader(fcf, Frame::kSecNone);
    frame->SetSrcPanId(mPanid);
    frame->SetSrcAddr(mAddress64);

    // write payload
    payload = frame->GetPayload();

    // Superframe Specification
    payload[0] = 0xff;
    payload[1] = 0x0f;
    payload += 2;

    // GTS Fields
    payload[0] = 0x00;
    payload++;

    // Pending Address Fields
    payload[0] = 0x00;
    payload++;

    // Protocol ID
    payload[0] = 0x03;
    payload++;

    // Version and Flags
    payload[0] = 0x1 << 4 | 0x1;
    payload++;

    // Network Name
    memcpy(payload, mNetworkName, sizeof(mNetworkName));
    payload += sizeof(mNetworkName);

    // Extended PAN
    memcpy(payload, mExtendedPanid, sizeof(mExtendedPanid));
    payload += sizeof(mExtendedPanid);

    frame->SetPayloadLength(payload - frame->GetPayload());

    dprintf("Sent Beacon\n");
}

void Mac::HandleBackoffTimer(void *context)
{
    Mac *obj = reinterpret_cast<Mac *>(context);
    obj->HandleBackoffTimer();
}

void Mac::ProcessTransmitSecurity()
{
    uint8_t securityLevel;
    uint8_t nonce[13];
    uint8_t tagLength;
    Crypto::AesEcb aesEcb;
    Crypto::AesCcm aesCcm;

    if (mSendFrame.GetSecurityEnabled() == false)
    {
        ExitNow();
    }

    mSendFrame.GetSecurityLevel(securityLevel);
    mSendFrame.SetFrameCounter(mKeyManager->GetMacFrameCounter());

    mSendFrame.SetKeyId((mKeyManager->GetCurrentKeySequence() & 0x7f) + 1);

    GenerateNonce(mAddress64, mKeyManager->GetMacFrameCounter(), securityLevel, nonce);

    aesEcb.SetKey(mKeyManager->GetCurrentMacKey(), 16);
    tagLength = mSendFrame.GetFooterLength() - 2;

    aesCcm.Init(aesEcb, mSendFrame.GetHeaderLength(), mSendFrame.GetPayloadLength(), tagLength,
                nonce, sizeof(nonce));
    aesCcm.Header(mSendFrame.GetHeader(), mSendFrame.GetHeaderLength());
    aesCcm.Payload(mSendFrame.GetPayload(), mSendFrame.GetPayload(), mSendFrame.GetPayloadLength(), true);
    aesCcm.Finalize(mSendFrame.GetFooter(), &tagLength);

    mKeyManager->IncrementMacFrameCounter();

exit:
    {}
}

void Mac::HandleBackoffTimer()
{
    ThreadError error = kThreadError_None;

    if (ot_radio_idle() != kThreadError_None)
    {
        mBackoffTimer.Start(16);
        ExitNow();
    }

    switch (mState)
    {
    case kStateActiveScan:
        mSendFrame.SetChannel(mScanChannel);
        SendBeaconRequest(&mSendFrame);
        mSendFrame.SetSequence(0);
        break;

    case kStateTransmitBeacon:
        mSendFrame.SetChannel(mChannel);
        SendBeacon(&mSendFrame);
        mSendFrame.SetSequence(mBeaconSequence++);
        break;

    case kStateTransmitData:
        mSendFrame.SetChannel(mChannel);
        SuccessOrExit(error = mSendHead->HandleFrameRequest(mSendFrame));
        mSendFrame.SetSequence(mDataSequence);
        break;

    default:
        assert(false);
        break;
    }

    // Security Processing
    ProcessTransmitSecurity();

    SuccessOrExit(error = ot_radio_transmit(&mSendFrame));

    if (mSendFrame.GetAckRequest())
    {
//#ifdef CPU_KW2X
        mAckTimer.Start(kMacAckTimeout);
//#endif
        dprintf("ack timer start\n");
    }

exit:

    if (error != kThreadError_None)
    {
        assert(false);
    }
}

extern "C" void ot_radio_signal_transmit_done()
{
    sTransmitDoneTask.Post();
}

void Mac::TransmitDoneTask(void *context)
{
    sMac->TransmitDoneTask();
}

void Mac::TransmitDoneTask()
{
    ThreadError error;
    bool rxPending;

    error = ot_radio_handle_transmit_done(&rxPending);

    mAckTimer.Stop();

    if (error != kThreadError_None)
    {
        mBackoffTimer.Start(16);
        ExitNow();
    }

    switch (mState)
    {
    case kStateActiveScan:
        mAckTimer.Start(mScanIntervalPerChannel);
        break;

    case kStateTransmitBeacon:
        SentFrame(true);
        break;

    case kStateTransmitData:
        if (rxPending)
        {
            mReceiveTimer.Start(kDataTimeout);
        }
        else
        {
            mReceiveTimer.Stop();
        }

        SentFrame(true);
        break;

    default:
        assert(false);
        break;
    }

exit:
    NextOperation();
}

void Mac::HandleAckTimer(void *context)
{
    Mac *obj = reinterpret_cast<Mac *>(context);
    obj->HandleAckTimer();
}

void Mac::HandleAckTimer()
{
    ot_radio_idle();

    switch (mState)
    {
    case kStateActiveScan:
        do
        {
            mScanChannelMask >>= 1;
            mScanChannel++;

            if (mScanChannelMask == 0 || mScanChannel > 26)
            {
                mActiveScanHandler(mActiveScanContext, NULL);
                ScheduleNextTransmission();
                ExitNow();
            }
        }
        while ((mScanChannelMask & 1) == 0);

        mBackoffTimer.Start(16);
        break;

    case kStateTransmitData:
        dprintf("ack timer fired\n");
        SentFrame(false);
        break;

    default:
        assert(false);
        break;
    }

exit:
    NextOperation();
}

void Mac::HandleReceiveTimer(void *context)
{
    Mac *obj = reinterpret_cast<Mac *>(context);
    obj->HandleReceiveTimer();
}

void Mac::HandleReceiveTimer()
{
    dprintf("data poll timeout!\n");
    NextOperation();
}

void Mac::SentFrame(bool acked)
{
    Address destination;
    Neighbor *neighbor;
    uint32_t backoff;
    Sender *sender;

    switch (mState)
    {
    case kStateActiveScan:
        mAckTimer.Start(mScanIntervalPerChannel);
        break;

    case kStateTransmitBeacon:
        ScheduleNextTransmission();
        break;

    case kStateTransmitData:
        if (mSendFrame.GetAckRequest() && !acked)
        {
            dump("NO ACK", mSendFrame.GetHeader(), 16);

            if (mAttempts < 12)
            {
                mAttempts++;
                backoff = (ot_random_get() % 32) + 1;
                mBackoffTimer.Start(backoff);
                ExitNow();
            }

            mSendFrame.GetDstAddr(destination);

            if ((neighbor = mMle->GetNeighbor(destination)) != NULL)
            {
                neighbor->mState = Neighbor::kStateInvalid;
            }
        }

        mAttempts = 0;

        sender = mSendHead;
        mSendHead = mSendHead->mNext;

        if (mSendHead == NULL)
        {
            mSendTail = NULL;
        }

        mDataSequence++;
        sender->HandleSentFrame(mSendFrame);

        ScheduleNextTransmission();
        break;

    default:
        assert(false);
        break;
    }

exit:
    {}
}

ThreadError Mac::ProcessReceiveSecurity(const Address &srcaddr, Neighbor &neighbor)
{
    ThreadError error = kThreadError_None;
    uint8_t securityLevel;
    uint32_t frameCounter;
    uint8_t nonce[13];
    uint8_t tag[16];
    uint8_t tagLength;
    uint8_t keyid;
    uint32_t keySequence;
    const uint8_t *macKey;
    Crypto::AesEcb aesEcb;
    Crypto::AesCcm aesCcm;

    if (mReceiveFrame.GetSecurityEnabled() == false)
    {
        ExitNow();
    }

    VerifyOrExit(mKeyManager != NULL, error = kThreadError_Security);

    mReceiveFrame.GetSecurityLevel(securityLevel);
    mReceiveFrame.GetFrameCounter(frameCounter);

    GenerateNonce(srcaddr.mAddress64, frameCounter, securityLevel, nonce);

    tagLength = mReceiveFrame.GetFooterLength() - 2;

    mReceiveFrame.GetKeyId(keyid);
    keyid--;

    if (keyid == (mKeyManager->GetCurrentKeySequence() & 0x7f))
    {
        // same key index
        keySequence = mKeyManager->GetCurrentKeySequence();
        macKey = mKeyManager->GetCurrentMacKey();
        VerifyOrExit(neighbor.mPreviousKey == true || frameCounter >= neighbor.mValid.mLinkFrameCounter,
                     error = kThreadError_Security);
    }
    else if (neighbor.mPreviousKey &&
             mKeyManager->IsPreviousKeyValid() &&
             keyid == (mKeyManager->GetPreviousKeySequence() & 0x7f))
    {
        // previous key index
        keySequence = mKeyManager->GetPreviousKeySequence();
        macKey = mKeyManager->GetPreviousMacKey();
        VerifyOrExit(frameCounter >= neighbor.mValid.mLinkFrameCounter, error = kThreadError_Security);
    }
    else if (keyid == ((mKeyManager->GetCurrentKeySequence() + 1) & 0x7f))
    {
        // next key index
        keySequence = mKeyManager->GetCurrentKeySequence() + 1;
        macKey = mKeyManager->GetTemporaryMacKey(keySequence);
    }
    else
    {
        for (Receiver *receiver = mReceiveHead; receiver; receiver = receiver->mNext)
        {
            receiver->HandleReceivedFrame(mReceiveFrame, kThreadError_Security);
        }

        ExitNow(error = kThreadError_Security);
    }

    aesEcb.SetKey(macKey, 16);
    aesCcm.Init(aesEcb, mReceiveFrame.GetHeaderLength(), mReceiveFrame.GetPayloadLength(),
                tagLength, nonce, sizeof(nonce));
    aesCcm.Header(mReceiveFrame.GetHeader(), mReceiveFrame.GetHeaderLength());
    aesCcm.Payload(mReceiveFrame.GetPayload(), mReceiveFrame.GetPayload(), mReceiveFrame.GetPayloadLength(),
                   false);
    aesCcm.Finalize(tag, &tagLength);

    VerifyOrExit(memcmp(tag, mReceiveFrame.GetFooter(), tagLength) == 0, error = kThreadError_Security);

    if (keySequence > mKeyManager->GetCurrentKeySequence())
    {
        mKeyManager->SetCurrentKeySequence(keySequence);
    }

    if (keySequence == mKeyManager->GetCurrentKeySequence())
    {
        neighbor.mPreviousKey = false;
    }

    neighbor.mValid.mLinkFrameCounter = frameCounter + 1;

exit:
    return error;
}

extern "C" void ot_radio_signal_receive_done()
{
    sReceiveDoneTask.Post();
}

void Mac::ReceiveDoneTask(void *context)
{
    sMac->ReceiveDoneTask();
}

void Mac::ReceiveDoneTask()
{
    ThreadError error;
    Address srcaddr;
    Address dstaddr;
    PanId panid;
    Neighbor *neighbor;
    int entry;
    int8_t rssi;

    error = ot_radio_handle_receive_done();
    VerifyOrExit(error == kThreadError_None, ;);

    mReceiveFrame.GetSrcAddr(srcaddr);
    neighbor = mMle->GetNeighbor(srcaddr);

    switch (srcaddr.mLength)
    {
    case 0:
        break;

    case 2:
        VerifyOrExit(neighbor != NULL, dprintf("drop not neighbor\n"));
        srcaddr.mLength = 8;
        memcpy(&srcaddr.mAddress64, &neighbor->mMacAddr, sizeof(srcaddr.mAddress64));
        break;

    case 8:
        break;

    default:
        ExitNow();
    }

    // Source Whitelist Processing
    if (srcaddr.mLength != 0 && mWhitelist.IsEnabled())
    {
        VerifyOrExit((entry = mWhitelist.Find(srcaddr.mAddress64)) >= 0, ;);

        if (mWhitelist.GetRssi(entry, rssi) == kThreadError_None)
        {
            mReceiveFrame.mPower = rssi;
        }
    }

    // Destination Address Filtering
    mReceiveFrame.GetDstAddr(dstaddr);

    switch (dstaddr.mLength)
    {
    case 0:
        break;

    case 2:
        mReceiveFrame.GetDstPanId(panid);
        VerifyOrExit((panid == kShortAddrBroadcast || panid == mPanid) &&
                     ((mRxOnWhenIdle && dstaddr.mAddress16 == kShortAddrBroadcast) ||
                      dstaddr.mAddress16 == mAddress16), ;);
        break;

    case 8:
        mReceiveFrame.GetDstPanId(panid);
        VerifyOrExit(panid == mPanid &&
                     memcmp(&dstaddr.mAddress64, &mAddress64, sizeof(dstaddr.mAddress64)) == 0, ;);
        break;
    }

    // Security Processing
    SuccessOrExit(ProcessReceiveSecurity(srcaddr, *neighbor));

    switch (mState)
    {
    case kStateActiveScan:
        HandleBeaconFrame();
        break;

    default:
        if (dstaddr.mLength != 0)
        {
            mReceiveTimer.Stop();
        }

        if (mReceiveFrame.GetType() == Frame::kFcfFrameMacCmd)
        {
            SuccessOrExit(HandleMacCommand());
        }

        for (Receiver *receiver = mReceiveHead; receiver; receiver = receiver->mNext)
        {
            receiver->HandleReceivedFrame(mReceiveFrame, kThreadError_None);
        }

        break;
    }

exit:
    NextOperation();
}

void Mac::HandleBeaconFrame()
{
    uint8_t *payload = mReceiveFrame.GetPayload();
    uint8_t payloadLength = mReceiveFrame.GetPayloadLength();
    ActiveScanResult result;
    Address address;

    if (mReceiveFrame.GetType() != Frame::kFcfFrameBeacon)
    {
        ExitNow();
    }

#if 0

    // Superframe Specification, GTS fields, and Pending Address fields
    if (payloadLength < 4 || payload[0] != 0xff || payload[1] != 0x0f || payload[2] != 0x00 || payload[3] != 0x00)
    {
        ExitNow();
    }

#endif
    payload += 4;
    payloadLength -= 4;

#if 0

    // Protocol ID
    if (payload[0] != 3)
    {
        ExitNow();
    }

#endif
    payload++;

    // skip Version and Flags
    payload++;

    // network name
    memcpy(result.mNetworkName, payload, sizeof(result.mNetworkName));
    payload += 16;

    // extended panid
    memcpy(result.mExtPanid, payload, sizeof(result.mExtPanid));
    payload += 8;

    // extended address
    mReceiveFrame.GetSrcAddr(address);
    memcpy(result.mExtAddr, &address.mAddress64, sizeof(result.mExtAddr));

    // panid
    mReceiveFrame.GetSrcPanId(result.mPanid);

    // channel
    result.mChannel = mReceiveFrame.GetChannel();

    // rssi
    result.mRssi = mReceiveFrame.GetPower();

    mActiveScanHandler(mActiveScanContext, &result);

exit:
    {}
}

ThreadError Mac::HandleMacCommand()
{
    ThreadError error = kThreadError_None;
    uint8_t commandId;
    mReceiveFrame.GetCommandId(commandId);

    if (commandId == Frame::kMacCmdBeaconRequest)
    {
        dprintf("Received Beacon Request\n");
        mTransmitBeacon = true;

        if (mState == kStateIdle)
        {
            mState = kStateTransmitBeacon;
            mTransmitBeacon = false;
            mBackoffTimer.Start(16);
        }

        ExitNow(error = kThreadError_Drop);
    }

exit:
    return error;
}

Whitelist *Mac::GetWhitelist()
{
    return &mWhitelist;
}

}  // namespace Mac
}  // namespace Thread
