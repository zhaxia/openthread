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
 *    @author  WenZheng Li  <wenzheng@nestlabs.com>
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
 */

#ifndef PLATFORM_EM35X_PHY_H_
#define PLATFORM_EM35X_PHY_H_

#include <common/tasklet.h>
#include <common/timer.h>
#include <platform/common/phy_interface.h>

#include <time.h>

namespace Thread {

class PhyPacket: public PhyPacketInterface {
 public:
  uint8_t *GetPsdu() final;

  uint8_t GetPsduLength() const final;
  void SetPsduLength(uint8_t psdu_length) final;

  uint8_t GetChannel() const final;
  void SetChannel(uint8_t channel) final;

  int8_t GetPower() const final;
  void SetPower(int8_t power) final;

 private:
  uint8_t packet_[128];
  uint8_t channel_;
  uint8_t power_;
};

class Phy: public PhyInterface {
 public:
  explicit Phy(Callbacks *callbacks);
  Error Start() final;
  Error Stop() final;

  Error Sleep() final;
  Error Idle() final;
  Error Receive(PhyPacket *packet) final;
  Error Transmit(PhyPacket *packet) final;

  Error SetPanId(uint16_t panid) final;
  Error SetExtendedAddress(uint8_t *address) final;
  Error SetShortAddress(uint16_t address) final;

  int8_t GetNoiseFloor() final;
  State GetState() final;

  void HandleGenEvent();
  void HandleReceiveEvent();
  void HandleReceivedFrame(uint8_t *packet);
  void HandleTransmitEvent();

 private:
  static void ReceivedTask(void *context);
  void ReceivedTask();
  static void SentTask(void *context);
  void SentTask();
  Tasklet received_task_;
  Tasklet sent_task_;

  State state_ = kStateDisabled;
  PhyPacket *receive_packet_ = NULL;
  PhyPacket *transmit_packet_ = NULL;
  Error transmit_error_ = kErrorNone;
  Error receive_error_ = kErrorNone;
};

}  // namespace Thread

#endif  // PLATFORM_EM35X_PHY_H_
