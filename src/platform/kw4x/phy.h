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

#ifndef PLATFORM_KW2X_PHY_H_
#define PLATFORM_KW2X_PHY_H_

#include "core/cpu.h"

#include <common/tasklet.h>
#include <common/timer.h>
#include <platform/common/phy_interface.h>
#include <bsp/ConnSW/ieee_802_15_4/Source/Phy/Interface/PhyInterface.h>

#include <time.h>

typedef pdDataReq_t phyPacket_t;

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

  phyPacket_t *GetPacket();

 private:
  phyPacket_t packet_;
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

  void PhyPlmeSyncLossIndication();
  void PhyTimeRxTimeoutIndication();
  void PhyTimeStartEventIndication();
  void PhyPlmeCcaConfirm(bool channelInUse);
  void PhyPlmeEdConfirm(uint8_t energyLevel);
  void PhyPdDataConfirm();
  void PhyPdDataIndication();
  void PhyPlmeFilterFailRx();
  void PhyPlmeRxSfdDetect(uint8_t frame_length);

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
  Error transmit_error_;
  Error receive_error_;
  phyRxParams_t rx_params_;
};

}  // namespace Thread

#endif  // PLATFORM_KW2X_PHY_H_
