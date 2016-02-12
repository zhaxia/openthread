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

#ifndef MAC_MAC_H_
#define MAC_MAC_H_

#include <common/tasklet.h>
#include <common/timer.h>
#include <crypto/aes_ecb.h>
#include <mac/mac_frame.h>
#include <mac/mac_whitelist.h>
#include <platform/common/phy.h>
#include <thread/key_manager.h>

namespace Thread {

enum {
  kMacAckTimeout = 16,  // milliseconds
  kDataTimeout = 100,  // milliseconds

  kMacScanChannelMaskAllChannels = 0xffff,
  kMacScanDefaultInterval = 128  // milliseconds
};

class Mac: PhyInterface::Callbacks {
 public:
  enum {
    kMaxBeacons = 16,
  };

  struct ActiveScanResult {
    uint8_t network_name[16];
    uint8_t ext_panid[8];
    uint8_t ext_addr[8];
    uint16_t panid;
    uint8_t channel;
    int8_t rssi;
  };

  class Receiver {
    friend class Mac;

   public:
    typedef void (*HandleReceivedFrame)(void *context, MacFrame *frame, ThreadError error);
    Receiver(HandleReceivedFrame handle_received_frame, void *context) {
      handle_received_frame_ = handle_received_frame;
      context_ = context;
      next_ = NULL;
    }

   private:
    HandleReceivedFrame handle_received_frame_;
    void *context_;
    Receiver *next_;
  };

  class Sender {
    friend class Mac;

   public:
    typedef ThreadError (*HandleFrameRequest)(void *context, MacFrame *frame);
    typedef void (*HandleSentFrame)(void *context, MacFrame *frame);
    Sender(HandleFrameRequest handle_frame_request, HandleSentFrame handle_sent_frame, void *context) {
      handle_frame_request_ = handle_frame_request;
      handle_sent_frame_ = handle_sent_frame;
      context_ = context;
      next_ = NULL;
    }

   private:
    HandleFrameRequest handle_frame_request_;
    HandleSentFrame handle_sent_frame_;
    void *context_;
    Sender *next_;
  };

  Mac(KeyManager *key_manager, MleRouter *mle);

  ThreadError Init();
  ThreadError Start();
  ThreadError Stop();

  // Channel bit mask: bit0 (lsb) -> ch11, bit1 -> ch12, ... , bit15 (msb) -> ch26,
  // value zero means all channels (same as 0xffff)
  typedef void (*HandleActiveScanResult)(void *context, ActiveScanResult *result);
  ThreadError ActiveScan(uint16_t interval_in_ms_per_channel, uint16_t channel_mask,
                         HandleActiveScanResult callback, void *context);

  bool GetRxOnWhenIdle() const;
  ThreadError SetRxOnWhenIdle(bool rx_on_when_idle);

  ThreadError RegisterReceiver(Receiver *receiver);
  ThreadError SendFrameRequest(Sender *sender);

  const MacAddr64 *GetAddress64() const;
  MacAddr16 GetAddress16() const;
  ThreadError SetAddress16(MacAddr16 address16);

  uint8_t GetChannel() const;
  ThreadError SetChannel(uint8_t channel);

  const char *GetNetworkName() const;
  ThreadError SetNetworkName(const char *name);

  uint16_t GetPanId() const;
  ThreadError SetPanId(uint16_t panid);

  const uint8_t *GetExtendedPanId() const;
  ThreadError SetExtendedPanId(const uint8_t *xpanid);

  void HandleReceiveDone(PhyPacket *packet, Phy::Error error) final;
  void HandleTransmitDone(PhyPacket *packet, bool rx_pending, Phy::Error error) final;

  MacWhitelist *GetWhitelist();

  Phy *GetPhy();

 private:
  void GenerateNonce(const MacAddr64 *address, uint32_t frame_counter, uint8_t security_level, uint8_t *nonce);
  void NextOperation();
  void SentFrame(bool acked);
  void SendBeaconRequest(MacFrame *frame);
  void SendBeacon(MacFrame *frame);
  void StartBackoff();
  void HandleBeaconFrame(MacFrame *frame);

  static void HandleAckTimer(void *context);
  void HandleAckTimer();
  static void HandleBackoffTimer(void *context);
  void HandleBackoffTimer();
  static void HandleReceiveTimer(void *context);
  void HandleReceiveTimer();

  Timer ack_timer_;
  Timer backoff_timer_;
  Timer receive_timer_;

  KeyManager *key_manager_ = NULL;

  MacAddr64 address64_;
  MacAddr16 address16_ = MacFrame::kShortAddrInvalid;
  uint16_t panid_ = MacFrame::kShortAddrInvalid;
  uint8_t extended_panid_[8];
  char network_name_[16];
  uint8_t channel_ = 12;

  MacFrame send_frame_;
  MacFrame receive_frame_;
  Sender *send_head_ = NULL, *send_tail_ = NULL;
  Receiver *receive_head_ = NULL, *receive_tail_ = NULL;
  MleRouter *mle_;
  Phy phy_;

  enum {
    kStateDisabled = 0,
    kStateIdle,
    kStateActiveScan,
    kStateTransmitBeacon,
    kStateTransmitData,
  };
  uint8_t state_ = kStateDisabled;

  uint8_t beacon_sequence_;
  uint8_t data_sequence_;
  bool rx_on_when_idle_ = true;
  uint8_t attempts_ = 0;
  bool transmit_beacon_ = false;

  bool active_scan_request_ = false;
  uint8_t scan_channel_ = 11;
  uint16_t scan_channel_mask_ = 0xff;
  uint16_t scan_interval_per_channel = 0;
  HandleActiveScanResult active_scan_callback_ = NULL;
  void *active_scan_context_ = NULL;

  MacWhitelist whitelist_;
};

}  // namespace Thread

#endif  // MAC_MAC_H_
