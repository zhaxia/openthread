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

#include <common/code_utils.h>
#include <common/random.h>
#include <common/thread_error.h>
#include <crypto/aes_ccm.h>
#include <mac/mac.h>
#include <mac/mac_frame.h>
#include <thread/mle_router.h>
#include <stdio.h>
#include <string.h>

namespace Thread {

static const uint8_t extended_panid_init_[] = {0xde, 0xad, 0x00, 0xbe, 0xef, 0x00, 0xca, 0xfe};
static const char network_name_init_[] = "JonathanHui";
static Mac *mac_;

Mac::Mac(KeyManager *key_manager, MleRouter *mle):
    ack_timer_(&HandleAckTimer, this),
    backoff_timer_(&HandleBackoffTimer, this),
    receive_timer_(&HandleReceiveTimer, this),
    phy_(this) {
  key_manager_ = key_manager;
  mle_ = mle;

  memcpy(extended_panid_, extended_panid_init_, sizeof(extended_panid_));
  memcpy(network_name_, network_name_init_, sizeof(network_name_));

  mac_ = this;
}

ThreadError Mac::Init() {
  for (int i = 0; i < sizeof(address64_); i++)
    address64_.bytes[i] = Random::Get();
  beacon_sequence_ = Random::Get();
  data_sequence_ = Random::Get();
  return kThreadError_None;
}

ThreadError Mac::Start() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(state_ == kStateDisabled, ;);

  VerifyOrExit(phy_.Start() == phy_.kErrorNone, error = kThreadError_Busy);

  SetExtendedPanId(extended_panid_);
  phy_.SetPanId(panid_);
  phy_.SetShortAddress(address16_);
  {
    uint8_t buf[8];
    for (int i = 0; i < sizeof(buf); i++)
      buf[i] = address64_.bytes[7-i];
    phy_.SetExtendedAddress(buf);
  }
  state_ = kStateIdle;
  NextOperation();

exit:
  return error;
}

ThreadError Mac::Stop() {
  ThreadError error = kThreadError_None;

  VerifyOrExit(phy_.Stop() == phy_.kErrorNone, error = kThreadError_Busy);
  ack_timer_.Stop();
  backoff_timer_.Stop();
  state_ = kStateDisabled;

  while (send_head_ != NULL) {
    Sender *cur;
    cur = send_head_;
    send_head_ = send_head_->next_;
    cur->next_ = NULL;
    cur->handle_sent_frame_(cur->context_, NULL);
  }
  send_tail_ = NULL;

  while (receive_head_ != NULL) {
    Receiver *cur;
    cur = receive_head_;
    receive_head_ = receive_head_->next_;
    cur->next_ = NULL;
  }
  receive_tail_ = NULL;

exit:
  return error;
}

ThreadError Mac::ActiveScan(uint16_t interval_per_channel, uint16_t channel_mask,
                            HandleActiveScanResult callback, void *context) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(state_ != kStateDisabled && state_ != kStateActiveScan && active_scan_request_ == false,
               error = kThreadError_Busy);

  active_scan_callback_ = callback;
  active_scan_context_ = context;
  scan_channel_mask_ = (channel_mask == 0) ? kMacScanChannelMaskAllChannels : channel_mask;
  scan_interval_per_channel = (interval_per_channel == 0) ? kMacScanDefaultInterval : interval_per_channel;

  scan_channel_ = 11;
  while ((scan_channel_mask_ & 1) == 0) {
    scan_channel_mask_ >>= 1;
    scan_channel_++;
  }

  if (state_ == kStateIdle) {
    state_ = kStateActiveScan;
    backoff_timer_.Start(16);
  } else {
    active_scan_request_ = true;
  }

exit:
  return error;
}

ThreadError Mac::RegisterReceiver(Receiver *receiver) {
  assert(receive_tail_ != receiver && receiver->next_ == NULL);

  if (receive_tail_ == NULL) {
    receive_head_ = receiver;
    receive_tail_ = receiver;
  } else {
    receive_tail_->next_ = receiver;
    receive_tail_ = receiver;
  }

  return kThreadError_None;
}

bool Mac::GetRxOnWhenIdle() const {
  return rx_on_when_idle_;
}

ThreadError Mac::SetRxOnWhenIdle(bool rx_on_when_idle) {
  rx_on_when_idle_ = rx_on_when_idle;
  return kThreadError_None;
}

const MacAddr64 *Mac::GetAddress64() const {
  return &address64_;
}

MacAddr16 Mac::GetAddress16() const {
  return address16_;
}

ThreadError Mac::SetAddress16(MacAddr16 address16) {
  address16_ = address16;
  phy_.SetShortAddress(address16);
  return kThreadError_None;
}

uint8_t Mac::GetChannel() const {
  return channel_;
}

ThreadError Mac::SetChannel(uint8_t channel) {
  channel_ = channel;
  return kThreadError_None;
}

const char *Mac::GetNetworkName() const {
  return network_name_;
}

ThreadError Mac::SetNetworkName(const char *name) {
  strncpy(network_name_, name, sizeof(network_name_));
  return kThreadError_None;
}

uint16_t Mac::GetPanId() const {
  return panid_;
}

ThreadError Mac::SetPanId(uint16_t panid) {
  panid_ = panid;
  phy_.SetPanId(panid_);
  return kThreadError_None;
}

const uint8_t *Mac::GetExtendedPanId() const {
  return extended_panid_;
}

ThreadError Mac::SetExtendedPanId(const uint8_t *xpanid) {
  memcpy(extended_panid_, xpanid, sizeof(extended_panid_));
  mle_->SetMeshLocalPrefix(extended_panid_);
  return kThreadError_None;
}

ThreadError Mac::SendFrameRequest(Sender *sender) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(state_ != kStateDisabled &&
               send_tail_ != sender && sender->next_ == NULL,
               error = kThreadError_Busy);

  if (send_head_ == NULL) {
    send_head_ = sender;
    send_tail_ = sender;
  } else {
    send_tail_->next_ = sender;
    send_tail_ = sender;
  }

  if (state_ == kStateIdle) {
    state_ = kStateTransmitData;
    uint32_t backoff = (Random::Get() % 32) + 1;
    backoff_timer_.Start(backoff);
  }

exit:
  return error;
}

void Mac::NextOperation() {
  switch (state_) {
    case kStateActiveScan:
      receive_frame_.SetChannel(scan_channel_);
      phy_.Receive(&receive_frame_);
      break;
    default:
      if (rx_on_when_idle_ || receive_timer_.IsRunning()) {
        receive_frame_.SetChannel(channel_);
        phy_.Receive(&receive_frame_);
      } else {
        phy_.Sleep();
      }
      break;
  }
}

void Mac::GenerateNonce(const MacAddr64 *address, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce) {
  // source address
  for (int i = 0; i < 8; i++)
    nonce[i] = address->bytes[i];
  nonce += 8;

  // frame counter
  nonce[0] = frame_counter >> 24;
  nonce[1] = frame_counter >> 16;
  nonce[2] = frame_counter >> 8;
  nonce[3] = frame_counter >> 0;
  nonce += 4;

  // security level
  nonce[0] = security_level;
}

void Mac::SendBeaconRequest(MacFrame *frame) {
  // initialize MAC header
  uint16_t fcf = MacFrame::kFcfFrameMacCmd | MacFrame::kFcfDstAddrShort | MacFrame::kFcfSrcAddrNone;
  frame->InitMacHeader(fcf, MacFrame::kSecNone);
  frame->SetDstPanId(MacFrame::kShortAddrBroadcast);
  frame->SetDstAddr(MacFrame::kShortAddrBroadcast);
  frame->SetCommandId(MacFrame::kMacCmdBeaconRequest);

  dprintf("Sent Beacon Request\n");
}

void Mac::SendBeacon(MacFrame *frame) {
  // initialize MAC header
  uint16_t fcf = MacFrame::kFcfFrameBeacon | MacFrame::kFcfDstAddrNone | MacFrame::kFcfSrcAddrExt;
  frame->InitMacHeader(fcf, MacFrame::kSecNone);
  frame->SetSrcPanId(panid_);
  frame->SetSrcAddr(&address64_);

  // write payload
  uint8_t *payload = frame->GetPayload();

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
  memcpy(payload, network_name_, sizeof(network_name_));
  payload += sizeof(network_name_);

  // Extended PAN
  memcpy(payload, extended_panid_, sizeof(extended_panid_));
  payload += sizeof(extended_panid_);

  frame->SetPayloadLength(payload - frame->GetPayload());

  dprintf("Sent Beacon\n");
}

void Mac::HandleBackoffTimer(void *context) {
  Mac *obj = reinterpret_cast<Mac*>(context);
  obj->HandleBackoffTimer();
}

void Mac::HandleBackoffTimer() {
  ThreadError error = kThreadError_None;

  if (phy_.Idle() != phy_.kErrorNone) {
    backoff_timer_.Start(16);
    return;
  }

  switch (state_) {
    case kStateActiveScan:
      send_frame_.SetChannel(scan_channel_);
      SendBeaconRequest(&send_frame_);
      send_frame_.SetSequence(0);
      break;
    case kStateTransmitBeacon:
      send_frame_.SetChannel(channel_);
      SendBeacon(&send_frame_);
      send_frame_.SetSequence(beacon_sequence_++);
      break;
    case kStateTransmitData:
      send_frame_.SetChannel(channel_);
      SuccessOrExit(error = send_head_->handle_frame_request_(send_head_->context_, &send_frame_));
      send_frame_.SetSequence(data_sequence_);
      break;
    default:
      printf("state = %d\n", state_);
      assert(false);
      break;
  }

  // Security Processing
  if (send_frame_.GetSecurityEnabled()) {
    uint8_t security_level;
    send_frame_.GetSecurityLevel(&security_level);
    send_frame_.SetFrameCounter(key_manager_->GetMacFrameCounter());

    uint8_t keyid;
    keyid = (key_manager_->GetCurrentKeySequence() & 0x7f) + 1;
    send_frame_.SetKeyId(keyid);

    uint8_t nonce[13];
    GenerateNonce(&address64_, key_manager_->GetMacFrameCounter(), security_level, nonce);

    uint8_t tag_length = send_frame_.GetFooterLength() - 2;

    AesEcb aes_ecb;
    aes_ecb.SetKey(key_manager_->GetCurrentMacKey(), 16);

    AesCcm aes_ccm;
    aes_ccm.Init(&aes_ecb, send_frame_.GetHeaderLength(), send_frame_.GetPayloadLength(), tag_length,
                 nonce, sizeof(nonce));
    aes_ccm.Header(send_frame_.GetHeader(), send_frame_.GetHeaderLength());
    aes_ccm.Payload(send_frame_.GetPayload(), send_frame_.GetPayload(), send_frame_.GetPayloadLength(), true);
    aes_ccm.Finalize(send_frame_.GetFooter(), &tag_length);

    key_manager_->IncrementMacFrameCounter();
  }

  VerifyOrExit(phy_.Transmit(&send_frame_) == phy_.kErrorNone, error = kThreadError_Busy);
  if (send_frame_.GetAckRequest()) {
#ifdef CPU_KW2X
    ack_timer_.Start(kMacAckTimeout);
#endif
    dprintf("ack timer start\n");
  }

exit:
  if (error != kThreadError_None)
    assert(false);
}

void Mac::HandleTransmitDone(PhyPacket *packet, bool rx_pending, Phy::Error error) {
  ack_timer_.Stop();

  if (error != phy_.kErrorNone) {
    backoff_timer_.Start(16);
    ExitNow();
  }

  switch (state_) {
    case kStateActiveScan:
      ack_timer_.Start(scan_interval_per_channel);
      break;

    case kStateTransmitBeacon:
      SentFrame(true);
      break;

    case kStateTransmitData:
      if (rx_pending)
        receive_timer_.Start(kDataTimeout);
      else
        receive_timer_.Stop();
      SentFrame(true);
      break;

    default:
      assert(false);
      break;
  }

exit:
  NextOperation();
}

void Mac::HandleAckTimer(void *context) {
  Mac *obj = reinterpret_cast<Mac*>(context);
  obj->HandleAckTimer();
}

void Mac::HandleAckTimer() {
  phy_.Idle();

  switch (state_) {
    case kStateActiveScan:
      do {
        scan_channel_mask_ >>= 1;
        scan_channel_++;

        if (scan_channel_mask_ == 0 || scan_channel_ > 26) {
          active_scan_callback_(active_scan_context_, NULL);
          if (active_scan_request_) {
            active_scan_request_ = false;
            state_ = kStateActiveScan;
            backoff_timer_.Start(16);
          } else if (transmit_beacon_) {
            transmit_beacon_ = false;
            state_ = kStateTransmitBeacon;
            backoff_timer_.Start(16);
          } else if (send_head_ != NULL) {
            state_ = kStateTransmitData;
            backoff_timer_.Start(16);
          } else {
            state_ = kStateIdle;
          }
          ExitNow();
        }
      } while ((scan_channel_mask_ & 1) == 0);

      backoff_timer_.Start(16);
      break;

    case kStateTransmitData:
      dprintf("ack timer fired\n");
      SentFrame(false);
      break;

    default:
      dprintf("state = %d\n", state_);
      assert(false);
      break;
  }

exit:
  NextOperation();
}

void Mac::HandleReceiveTimer(void *context) {
  Mac *obj = reinterpret_cast<Mac*>(context);
  obj->HandleReceiveTimer();
}

void Mac::HandleReceiveTimer() {
  dprintf("data poll timeout!\n");
  NextOperation();
}

void Mac::SentFrame(bool acked) {
  switch (state_) {
    case kStateActiveScan:
      ack_timer_.Start(scan_interval_per_channel);
      break;

    case kStateTransmitBeacon:
      if (active_scan_request_) {
        active_scan_request_ = false;
        state_ = kStateActiveScan;
        backoff_timer_.Start(16);
      } else if (transmit_beacon_) {
        transmit_beacon_ = false;
        state_ = kStateTransmitBeacon;
        backoff_timer_.Start(16);
      } else if (send_head_ != NULL) {
        state_ = kStateTransmitData;
        backoff_timer_.Start(16);
      } else {
        state_ = kStateIdle;
      }
      break;

    case kStateTransmitData:
      if (send_frame_.GetAckRequest() && !acked) {
        dump("NO ACK", send_frame_.GetHeader(), 16);
        if (attempts_ < 12) {
          attempts_++;
          uint32_t backoff = (Random::Get() % 32) + 1;
          backoff_timer_.Start(backoff);
          return;
        }

        MacAddress destination;
        send_frame_.GetDstAddr(&destination);

        Neighbor *neighbor = mle_->GetNeighbor(&destination);
        if (neighbor != NULL)
          neighbor->state = Neighbor::kStateInvalid;
      }

      attempts_ = 0;

      Sender *sender;
      sender = send_head_;
      send_head_ = send_head_->next_;
      if (send_head_ == NULL)
        send_tail_ = NULL;

      data_sequence_++;
      sender->handle_sent_frame_(sender->context_, &send_frame_);

      if (active_scan_request_) {
        active_scan_request_ = false;
        state_ = kStateActiveScan;
        backoff_timer_.Start(16);
      } else if (transmit_beacon_) {
        transmit_beacon_ = false;
        state_ = kStateTransmitBeacon;
        backoff_timer_.Start(16);
      } else if (send_head_ != NULL) {
        state_ = kStateTransmitData;
        backoff_timer_.Start(16);
      } else {
        state_ = kStateIdle;
      }

      break;

    default:
      assert(false);
      break;
  }
}

void Mac::HandleReceiveDone(PhyPacket *packet, Phy::Error error) {
  assert(packet == &receive_frame_);

  VerifyOrExit(error == phy_.kErrorNone, ;);

  MacAddress srcaddr;
  receive_frame_.GetSrcAddr(&srcaddr);

  Neighbor *neighbor;
  neighbor = mle_->GetNeighbor(&srcaddr);

  switch (srcaddr.length) {
    case 0:
      break;
    case 2:
      VerifyOrExit(neighbor != NULL, dprintf("drop not neighbor\n"));
      srcaddr.length = 8;
      memcpy(&srcaddr.address64, &neighbor->mac_addr, sizeof(srcaddr.address64));
      break;
    case 8:
      break;
    default:
      ExitNow();
  }

  // Source Whitelist Processing
  if (srcaddr.length != 0 && whitelist_.IsEnabled()) {
    int entry;
    int8_t rssi;

    VerifyOrExit((entry = whitelist_.Find(&srcaddr.address64)) >= 0, ;);
    if (whitelist_.GetRssi(entry, &rssi) == kThreadError_None)
      packet->SetPower(rssi);
  }

  // Destination Address Filtering
  uint16_t panid;
  MacAddress dstaddr;
  receive_frame_.GetDstAddr(&dstaddr);

  switch (dstaddr.length) {
    case 0:
      break;
    case 2:
      receive_frame_.GetDstPanId(&panid);
      VerifyOrExit((panid == MacFrame::kShortAddrBroadcast || panid == panid_) &&
                   ((rx_on_when_idle_ && dstaddr.address16 == MacFrame::kShortAddrBroadcast) ||
                    dstaddr.address16 == address16_), ;);
      break;
    case 8:
      receive_frame_.GetDstPanId(&panid);
      VerifyOrExit(panid == panid_ &&
                   memcmp(&dstaddr.address64, &address64_, sizeof(dstaddr.address64)) == 0, ;);
      break;
  }

  uint8_t sequence;
  receive_frame_.GetSequence(&sequence);

  // Security Processing
  if (receive_frame_.GetSecurityEnabled()) {
    VerifyOrExit(neighbor != NULL && key_manager_ != NULL, ;);

    uint8_t security_level;
    receive_frame_.GetSecurityLevel(&security_level);

    uint32_t frame_counter;
    receive_frame_.GetFrameCounter(&frame_counter);

    uint8_t nonce[13];
    GenerateNonce(&srcaddr.address64, frame_counter, security_level, nonce);

    uint8_t tag[16];
    uint8_t tag_length = receive_frame_.GetFooterLength() - 2;

    uint8_t keyid;
    receive_frame_.GetKeyId(&keyid);
    keyid--;

    uint32_t key_sequence;
    const uint8_t *mac_key;
    if (keyid == (key_manager_->GetCurrentKeySequence() & 0x7f)) {
      // same key index
      key_sequence = key_manager_->GetCurrentKeySequence();
      mac_key = key_manager_->GetCurrentMacKey();
      VerifyOrExit(neighbor->previous_key == true || frame_counter >= neighbor->valid.link_frame_counter,
                   dprintf("mac frame counter reject %d %d\n", frame_counter, neighbor->valid.link_frame_counter));
    } else if (neighbor->previous_key &&
               key_manager_->IsPreviousKeyValid() &&
               keyid == (key_manager_->GetPreviousKeySequence() & 0x7f)) {
      // previous key index
      key_sequence = key_manager_->GetPreviousKeySequence();
      mac_key = key_manager_->GetPreviousMacKey();
      VerifyOrExit(frame_counter >= neighbor->valid.link_frame_counter,
                   dprintf("mac frame counter reject %d %d\n", frame_counter, neighbor->valid.link_frame_counter));
    } else if (keyid == ((key_manager_->GetCurrentKeySequence() + 1) & 0x7f)) {
      // next key index
      key_sequence = key_manager_->GetCurrentKeySequence() + 1;
      mac_key = key_manager_->GetTemporaryMacKey(key_sequence);
    } else {
      for (Receiver *receiver = receive_head_; receiver; receiver = receiver->next_)
        receiver->handle_received_frame_(receiver->context_, &receive_frame_, kThreadError_Security);
      ExitNow();
    }

    AesEcb aes_ecb;
    aes_ecb.SetKey(mac_key, 16);

    AesCcm aes_ccm;
    aes_ccm.Init(&aes_ecb, receive_frame_.GetHeaderLength(), receive_frame_.GetPayloadLength(),
                 tag_length, nonce, sizeof(nonce));
    aes_ccm.Header(receive_frame_.GetHeader(), receive_frame_.GetHeaderLength());
    aes_ccm.Payload(receive_frame_.GetPayload(), receive_frame_.GetPayload(), receive_frame_.GetPayloadLength(),
                    false);
    aes_ccm.Finalize(tag, &tag_length);

    VerifyOrExit(memcmp(tag, receive_frame_.GetFooter(), tag_length) == 0, ;; dprintf("mac verify fail\n"));
    if (key_sequence > key_manager_->GetCurrentKeySequence())
      key_manager_->SetCurrentKeySequence(key_sequence);
    if (key_sequence == key_manager_->GetCurrentKeySequence())
      neighbor->previous_key = false;

    neighbor->valid.link_frame_counter = frame_counter + 1;
  }

  switch (state_) {
    case kStateActiveScan:
      HandleBeaconFrame(&receive_frame_);
      break;

    default:
      if (dstaddr.length != 0)
        receive_timer_.Stop();

      if (receive_frame_.GetType() == MacFrame::kFcfFrameMacCmd) {
        uint8_t command_id;
        receive_frame_.GetCommandId(&command_id);
        if (command_id == MacFrame::kMacCmdBeaconRequest) {
          dprintf("Received Beacon Request\n");
          transmit_beacon_ = true;
          if (state_ == kStateIdle) {
            state_ = kStateTransmitBeacon;
            transmit_beacon_ = false;
            backoff_timer_.Start(16);
          }
          ExitNow();
        }
      }

      for (Receiver *receiver = receive_head_; receiver; receiver = receiver->next_)
        receiver->handle_received_frame_(receiver->context_, &receive_frame_, kThreadError_None);

      break;
  }

exit:
  NextOperation();
}

void Mac::HandleBeaconFrame(MacFrame *frame) {
  switch (frame->GetType()) {
    case MacFrame::kFcfFrameBeacon: {
      uint8_t *payload = frame->GetPayload();
      uint8_t payload_length = frame->GetPayloadLength();
      ActiveScanResult result;

#if 0
      // Superframe Specification, GTS fields, and Pending Address fields
      if (payload_length < 4 || payload[0] != 0xff || payload[1] != 0x0f || payload[2] != 0x00 || payload[3] != 0x00)
        break;
#endif
      payload += 4;
      payload_length -= 4;

#if 0
      // Protocol ID
      if (payload[0] != 3)
        break;
#endif
      payload++;

      // skip Version and Flags
      payload++;

      // network name
      memcpy(result.network_name, payload, sizeof(result.network_name));
      payload += 16;

      // extended panid
      memcpy(result.ext_panid, payload, sizeof(result.ext_panid));
      payload += 8;

      // extended address
      MacAddress address;
      frame->GetSrcAddr(&address);
      memcpy(result.ext_addr, &address.address64, sizeof(result.ext_addr));

      // panid
      frame->GetSrcPanId(&result.panid);

      // channel
      result.channel = frame->GetChannel();

      // rssi
      result.rssi = frame->GetPower();

      active_scan_callback_(active_scan_context_, &result);
      break;
    }
    case MacFrame::kFcfFrameMacCmd:
      break;
    default:
      break;
  }
}

MacWhitelist *Mac::GetWhitelist() {
  return &whitelist_;
}

Phy *Mac::GetPhy() {
  return &phy_;
}


}  // namespace Thread
