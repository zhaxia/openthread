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

#include <platform/kw2x/phy.h>
#include <common/code_utils.h>
#include <common/tasklet.h>
#include <common/timer.h>
#include <mac/mac_frame.h>
#include <platform/kw2x/alarm.h>
#include <cpu/CpuGpio.hpp>

namespace Thread {

static nl::Thread::CpuGpio theLed0(3, 4, 1);

uint8_t phy_events_[64];
uint8_t phy_events_cur_ = 0;

Phy *phy_ = NULL;

uint8_t *PhyPacket::GetPsdu() {
  return packet_.data;
}

uint8_t PhyPacket::GetPsduLength() const {
  return packet_.frameLength;
}

void PhyPacket::SetPsduLength(uint8_t psdu_length) {
  packet_.frameLength = psdu_length;
}

uint8_t PhyPacket::GetChannel() const {
  return channel_;
}

void PhyPacket::SetChannel(uint8_t channel) {
  channel_ = channel;
}

int8_t PhyPacket::GetPower() const {
  return power_;
}

void PhyPacket::SetPower(int8_t power) {
  power_ = power;
}

phyPacket_t *PhyPacket::GetPacket() {
  return &packet_;
}

/*
 * Name: mClkSwitchDelayTime_c
 * Description: Time in milliseconds required by the transceiver to switch
 *              the CLK_OUT clock frequency (in our case from 32KHz to 4 MHz)
 */
#define mClkSwitchDelayTime_c           50
#define mRst_B_AssertTime_c             50

/*
 * Name: mCLK_OUT_DIV_4MHz_c
 * Description: CLK_OUT_DIV field value for 4 MHz clock out frequency
 */
#define mCLK_OUT_DIV_4MHz_c             3

static void delayMs(uint16_t val) {
  uint32_t now = Alarm::GetNow();
  while (Alarm::GetNow() - now <= val) {}
}

static void HandleLedTimer(void *context) {
  theLed0.hi();
}

static Timer ledTimer(&HandleLedTimer, NULL);

Phy::Phy(Callbacks *callbacks):
    PhyInterface(callbacks),
    received_task_(&ReceivedTask, this),
    sent_task_(&SentTask, this) {
}

Phy::Error Phy::SetPanId(uint16_t panid) {
  uint8_t buf[2];
  buf[0] = panid >> 0;
  buf[1] = panid >> 8;
  PhyPpSetPanIdPAN0(buf);
  return kErrorNone;
}

Phy::Error Phy::SetExtendedAddress(uint8_t *address) {
  PhyPpSetLongAddrPAN0(address);
  return kErrorNone;
}

Phy::Error Phy::SetShortAddress(uint16_t address) {
  uint8_t buf[2];
  buf[0] = address >> 0;
  buf[1] = address >> 8;
  PhyPpSetShortAddrPAN0(buf);
  return kErrorNone;
}

Phy::Error Phy::Start() {
  theLed0.init();

  /* Initialize the transceiver SPI driver */
  MC1324xDrv_SPIInit();
  /* Configure the transceiver IRQ_B port */
  MC1324xDrv_IRQ_PortConfig();

  /* Configure the transceiver RST_B port */
  MC1324xDrv_RST_B_PortConfig();

  /* Transceiver Hard/RST_B RESET */
  MC1324xDrv_RST_B_Assert();
  delayMs(mRst_B_AssertTime_c);
  MC1324xDrv_RST_B_Deassert();

  /* Wait for transceiver to deassert IRQ pin */
  while (MC1324xDrv_IsIrqPending()) {}
  /* Wait for transceiver wakeup from POR iterrupt */
  while (!MC1324xDrv_IsIrqPending()) {}

  /* Enable transceiver SPI interrupt request */
  NVIC_EnableIRQ(MC1324x_Irq_Number);

  NVIC_SetPriority(MC1324x_Irq_Number, MC1324x_Irq_Priority);

  /* Enable the transceiver IRQ_B interrupt request */
  MC1324xDrv_IRQ_Enable();

  MC1324xDrv_Set_CLK_OUT_Freq(gCLK_OUT_FREQ_4_MHz);

  /* wait until the external reference clock is stable */
  delayMs(mClkSwitchDelayTime_c);

  PhyInit();
  PhyPlmeSetLQIModeRequest(1);  // LQI Based on RSSI

  state_ = kStateSleep;
  phy_ = this;

  return kErrorNone;
}

Phy::Error Phy::Stop() {
  MC1324xDrv_IRQ_Disable();
  MC1324xDrv_RST_B_Assert();
  state_ = kStateDisabled;
  return kErrorNone;
}

Phy::Error Phy::Sleep() {
  Error error = kErrorNone;

  MC1324xDrv_IRQ_Disable();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateSleep;

exit:
  MC1324xDrv_IRQ_Enable();
  return error;
}

Phy::Error Phy::Idle() {
  Error error = kErrorNone;

  MC1324xDrv_IRQ_Disable();
  switch (state_) {
    case kStateSleep:
      state_ = kStateIdle;
      break;
    case kStateIdle:
      break;
    case kStateListen:
    case kStateTransmit:
      PhyAbort();
      state_ = kStateIdle;
      break;
    case kStateDisabled:
    case kStateReceive:
      ExitNow(error = kErrorInvalidState);
      break;
  }

exit:
  MC1324xDrv_IRQ_Enable();
  return error;
}

Phy::Error Phy::Receive(PhyPacket *packet) {
  Error error = kErrorNone;

  MC1324xDrv_IRQ_Disable();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateListen;
  receive_packet_ = packet;
  VerifyOrExit(PhyPlmeSetCurrentChannelRequestPAN0(packet->GetChannel()) == gPhySuccess_c,
               error = kErrorInvalidState);
  VerifyOrExit(PhyPlmeRxRequest(receive_packet_->GetPacket(), 0, &rx_params_) == gPhySuccess_c,
               error = kErrorInvalidState);
  phy_events_[phy_events_cur_++] = 0x10;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;

exit:
  MC1324xDrv_IRQ_Enable();
  return error;
}

uint8_t GetPhyTxMode(PhyPacket *packet) {
  return reinterpret_cast<MacFrame*>(packet)->GetAckRequest() ?
  gDataReq_Ack_Cca_Unslotted_c : gDataReq_NoAck_Cca_Unslotted_c;
}

Phy::Error Phy::Transmit(PhyPacket *packet) {
  Error error = kErrorNone;

  MC1324xDrv_IRQ_Disable();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateTransmit;
  transmit_packet_ = packet;
  VerifyOrExit(PhyPlmeSetCurrentChannelRequestPAN0(packet->GetChannel()) == gPhySuccess_c,
               error = kErrorInvalidState);
  VerifyOrExit(PhyPdDataRequest(transmit_packet_->GetPacket(), GetPhyTxMode(transmit_packet_), NULL) ==
               gPhySuccess_c, error = kErrorInvalidState);
  phy_events_[phy_events_cur_++] = 0x11;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  theLed0.lo();
  ledTimer.Start(100);

exit:
  assert(error == kErrorNone);
  MC1324xDrv_IRQ_Enable();
  return error;
}

Phy::State Phy::GetState() {
  return state_;
}

int8_t Phy::GetNoiseFloor() {
  return 0;
}

void Phy::PhyPlmeSyncLossIndication() {
  switch (state_) {
    case kStateListen:
      receive_error_ = kErrorAbort;
      PhyAbort();
      state_ = kStateReceive;
      received_task_.Post();
      break;
    default:
      assert(false);
      break;
  }
}

void Phy::PhyTimeRxTimeoutIndication() {
  assert(false);
}

void Phy::PhyTimeStartEventIndication() {
  assert(false);
}

void Phy::PhyPlmeCcaConfirm(bool channelInUse) {
  switch (state_) {
    case kStateTransmit:
      transmit_error_ = kErrorAbort;
      sent_task_.Post();
      break;
    default:
      assert(false);
      break;
  }
}

void Phy::PhyPlmeEdConfirm(uint8_t energyLevel) {
  assert(false);
}

void Phy::PhyPdDataConfirm() {
  assert(state_ == kStateTransmit);
  transmit_error_ = kErrorNone;
  sent_task_.Post();
}

void Phy::PhyPdDataIndication() {
  assert(state_ == kStateListen || state_ == kStateReceive);
  state_ = kStateReceive;

  int rssi;
  rssi = rx_params_.linkQuality;
  rssi = ((rssi * 105) / 255) - 105;

  receive_packet_->SetPower(rssi);

  receive_error_ = kErrorNone;
  received_task_.Post();
}

void Phy::PhyPlmeFilterFailRx() {
#if 1
  switch (state_) {
    case kStateListen:
    case kStateReceive:
      PhyAbort();
      PhyPlmeRxRequest(receive_packet_->GetPacket(), 0, &rx_params_);
      state_ = kStateListen;
      break;
    case kStateTransmit:
      break;
    default:
      assert(false);
      break;
  }
#else
  switch (state_) {
    case kStateListen:
      break;
    case kStateReceive:
      state_ = kStateListen;
      break;
    case kStateTransmit:
      break;
    default:
      assert(false);
      break;
  }
#endif
}

uint8_t framelen_;

void Phy::PhyPlmeRxSfdDetect(uint8_t frameLen) {
  framelen_ = PhyPpGetState();
  switch (state_) {
    case kStateListen:
      state_ = kStateReceive;
      break;
    case kStateReceive:
      break;
    case kStateTransmit:
      break;
    default:
      assert(false);
      break;
  }
}

extern "C" void PhyPlmeSyncLossIndication() {
  phy_events_[phy_events_cur_++] = 1;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyPlmeSyncLossIndication();
}

extern "C" void PhyTimeRxTimeoutIndication() {
  phy_events_[phy_events_cur_++] = 2;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyTimeRxTimeoutIndication();
}

extern "C" void PhyTimeStartEventIndication() {
  phy_events_[phy_events_cur_++] = 3;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyTimeStartEventIndication();
}

extern "C" void PhyPlmeCcaConfirm(bool_t channelInUse) {
  phy_events_[phy_events_cur_++] = 4;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyPlmeCcaConfirm(channelInUse);
}

extern "C" void PhyPlmeEdConfirm(uint8_t energyLevel) {
  phy_events_[phy_events_cur_++] = 5;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyPlmeEdConfirm(energyLevel);
}

extern "C" void PhyPdDataConfirm() {
  phy_events_[phy_events_cur_++] = 6;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyPdDataConfirm();
}

extern "C" void PhyPdDataIndication() {
  phy_events_[phy_events_cur_++] = 7;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyPdDataIndication();
}

extern "C" void PhyPlmeFilterFailRx() {
  phy_events_[phy_events_cur_++] = 8;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyPlmeFilterFailRx();
}

extern "C" void PhyPlmeRxSfdDetect(uint8_t frame_length) {
  phy_events_[phy_events_cur_++] = 9;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  phy_->PhyPlmeRxSfdDetect(frame_length);
}

extern "C" void PhyUnexpectedTransceiverReset() {
  phy_events_[phy_events_cur_++] = 10;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;
  while (1) {}
}

void Phy::SentTask(void *context) {
  Phy *obj = reinterpret_cast<Phy*>(context);
  obj->SentTask();
}

void Phy::SentTask() {
  assert(state_ == kStateTransmit);
  state_ = kStateIdle;
  callbacks_->HandleTransmitDone(transmit_packet_, PhyPpIsRxAckDataPending(), transmit_error_);
}

void Phy::ReceivedTask(void *context) {
  Phy *obj = reinterpret_cast<Phy*>(context);
  obj->ReceivedTask();
}

void Phy::ReceivedTask() {
  assert(state_ == kStateReceive);
  state_ = kStateIdle;
  callbacks_->HandleReceiveDone(receive_packet_, receive_error_);
}

}  // namespace Thread
