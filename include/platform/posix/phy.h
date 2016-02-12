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

#ifndef PLATFORM_POSIX_PHY_H_
#define PLATFORM_POSIX_PHY_H_

#include <common/tasklet.h>
#include <platform/common/phy_interface.h>
#include <pthread.h>

#include <time.h>

namespace Thread {

class MacFrame;

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
  uint8_t psdu_length_ = 0;
  uint8_t psdu_[kMaxPsduLength];
  uint8_t channel_ = 0;
  int8_t power_ = 0;
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

 private:
  static void ReceivedTask(void *context);
  void ReceivedTask();

  static void SentTask(void *context);
  void SentTask();

  static void *ReceiveThread(void *arg);
  void ReceiveThread();

  Tasklet received_task_;
  Tasklet sent_task_;

  State state_ = kStateDisabled;
  PhyPacket *receive_packet_ = NULL;
  PhyPacket *transmit_packet_ = NULL;
  PhyPacket ack_packet_;
  bool data_pending_ = false;

  uint8_t extended_address_[8];
  uint16_t short_address_;
  uint16_t panid_;

  pthread_t thread_;
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condition_variable_ = PTHREAD_COND_INITIALIZER;
  int sockfd_;
  time_t last_update_time_;
};

}  // namespace Thread

#endif  // PLATFORM_POSIX_PHY_H_
