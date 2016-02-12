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
#include <common/tasklet.h>
#include <mac/mac_frame.h>
#include <platform/kw4x/phy.h>
#include <platform/kw4x/alarm.h>

#include <bsp/ConnSW/ieee_802_15_4/Source/Phy/Interface/Phy.h>

extern "C" {
void UnprotectFromXcvrInterrupt(void);
void ProtectFromXcvrInterrupt(void);
}

extern Phy_PhyLocalStruct_t     phyLocal;

event_t                  gTaskEvent;


namespace Thread {

uint8_t phy_events_[64];
uint8_t phy_events_cur_ = 0;

Phy *phy_ = NULL;

uint8_t *PhyPacket::GetPsdu() {
  return packet_.pPsdu;
}

uint8_t PhyPacket::GetPsduLength() const {
  return packet_.psduLength;
}

void PhyPacket::SetPsduLength(uint8_t psdu_length) {
  packet_.psduLength = psdu_length;
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

#if 0
static void delayMs(uint16_t val) {
  uint32_t now = Alarm::GetNow();
  while (Alarm::GetNow() - now <= val) {}
}
#endif

Phy::Phy(Callbacks *callbacks):
    PhyInterface(callbacks),
    received_task_(&ReceivedTask, this),
    sent_task_(&SentTask, this) {
}

Phy::Error Phy::SetPanId(uint16_t panid) {
  uint8_t buf[2];
  buf[0] = panid >> 0;
  buf[1] = panid >> 8;
  PhyPpSetPanId(buf,0);
  return kErrorNone;
}

Phy::Error Phy::SetExtendedAddress(uint8_t *address) {
  PhyPpSetLongAddr(address,0);
  return kErrorNone;
}

Phy::Error Phy::SetShortAddress(uint16_t address) {
  uint8_t buf[2];
  buf[0] = address >> 0;
  buf[1] = address >> 8;
  PhyPpSetShortAddr(buf,0);
  return kErrorNone;
}

Phy::Error Phy::Start() {
    Phy_Init();

#if 0
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
  UnprotectFromXcvrInterrupt();

  MC1324xDrv_Set_CLK_OUT_Freq(gCLK_OUT_FREQ_4_MHz);

  /* wait until the external reference clock is stable */
  delayMs(mClkSwitchDelayTime_c);

  PhyInit();
  PhyPlmeSetLQIModeRequest(1);  // LQI Based on RSSI
#endif

  state_ = kStateSleep;
  phy_ = this;

  return kErrorNone;
}

Phy::Error Phy::Stop() {
  ProtectFromXcvrInterrupt();
  //MC1324xDrv_RST_B_Assert();
  state_ = kStateDisabled;
  return kErrorNone;
}

Phy::Error Phy::Sleep() {
  Error error = kErrorNone;

  ProtectFromXcvrInterrupt();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateSleep;

exit:
  UnprotectFromXcvrInterrupt();
  return error;
}

Phy::Error Phy::Idle() {
  Error error = kErrorNone;

  ProtectFromXcvrInterrupt();
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
  UnprotectFromXcvrInterrupt();
  return error;
}

Phy::Error Phy::Receive(PhyPacket *packet) {
  Error error = kErrorNone;

  ProtectFromXcvrInterrupt();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateListen;
  receive_packet_ = packet;
  VerifyOrExit(PhyPlmeSetCurrentChannelRequest(packet->GetChannel(),0) == gPhySuccess_c,
               error = kErrorInvalidState);
  // TODO: strap in rxBuffer
  // receive_packet_->GetPacket(), 0, 
  phyLocal.rxParams.duration = 0xFFFFFFFF;
  phyLocal.rxParams.phyRxMode = gPhyUnslottedMode_c;
  phyLocal.rxParams.pRxData = 
    (pdDataToMacMessage_t*)receive_packet_->GetPacket();
  //PhyPlmeRxRequest(gPhyUnslottedMode_c, (phyRxParams_t*)&phyLocal.rxParams);
  VerifyOrExit(PhyPlmeRxRequest(gPhyUnslottedMode_c, 
				&rx_params_) == gPhySuccess_c,
               error = kErrorInvalidState);
  phy_events_[phy_events_cur_++] = 0x10;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;

exit:
  UnprotectFromXcvrInterrupt();
  return error;
}

phyTxParams_t GetPhyTxMode(PhyPacket *packet) {
  phyPacket_t *phyPacket = packet->GetPacket();
  phyTxParams_t txParams = 
  {
    .numOfCca = (phyPacket->CCABeforeTx) ? (uint8_t)0 : (uint8_t)1,
    .ackRequired = phyPacket->ackRequired
  };
  return txParams;
}

Phy::Error Phy::Transmit(PhyPacket *packet) {
  Error error = kErrorNone;
  phyTxParams_t txParams = GetPhyTxMode(packet);

  ProtectFromXcvrInterrupt();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateTransmit;
  transmit_packet_ = packet;
  VerifyOrExit(PhyPlmeSetCurrentChannelRequest(packet->GetChannel(),0) 
	       == gPhySuccess_c, error = kErrorInvalidState);

  VerifyOrExit(PhyPdDataRequest(packet->GetPacket(), NULL, &txParams) 
	       == gPhySuccess_c, error = kErrorInvalidState);


  phy_events_[phy_events_cur_++] = 0x11;
  if (phy_events_cur_ >= sizeof(phy_events_))
    phy_events_cur_ = 0;

exit:
  assert(error == kErrorNone);
  UnprotectFromXcvrInterrupt();
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
      // TODO: strap in rxBuffer
      // receive_packet_->GetPacket(), 0, 
      PhyPlmeRxRequest(gPhyUnslottedMode_c, &rx_params_);
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
