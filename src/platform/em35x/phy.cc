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

#include <platform/em35x/phy.h>
#include <common/code_utils.h>
#include <common/tasklet.h>
#include <mac/mac_frame.h>
#include <cpu/CpuGpio.hpp>
#include <common/random.h>

#define	PHY_EM3XX	  
#define CORTEXM3	      
#define CORTEXM3_EMBER_MICRO

extern "C" {
#include "bsp/phy/ember-shim.h"
#include "bsp/phy/phy.h"
}

extern "C" EmberStatus emApiSetTxPowerMode(uint8_t);
extern "C" EmberStatus emRadioCheckRadio();
extern "C" EmberStatus emberCalibrateCurrentChannel();
extern "C" void calStartAdcConversion(uint8_t, uint8_t);
extern "C" uint16_t calReadAdcBlocking();
extern "C" void calDisableAdc();

nl::Thread::CpuGpio femCsd(0,0,1);
nl::Thread::CpuGpio femCps(1,5,1);
nl::Thread::CpuGpio femCtx(2,5,1);

namespace Thread {

uint8_t phy_events_[64];
uint8_t phy_events_cur_ = 0;

Phy *phy_ = NULL;

uint8_t *PhyPacket::GetPsdu() {
  return packet_ + 1;
}

uint8_t PhyPacket::GetPsduLength() const {
  return packet_[0];
}

void PhyPacket::SetPsduLength(uint8_t psdu_length) {
  packet_[0] = psdu_length;
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

Phy::Phy(Callbacks *callbacks):
    PhyInterface(callbacks),
    received_task_(&ReceivedTask, this),
    sent_task_(&SentTask, this) {
  phy_ = this;
  calStartAdcConversion(CAL_ADC_CHANNEL_GND, ADC_SAMPLE_CLOCKS_32);
  for(int i = 0; i < 10; i++)
    Random::Init(Random::Get() ^ calReadAdcBlocking());
  calDisableAdc();
}

Phy::Error Phy::SetPanId(uint16_t panid) {
  emRadioSetPanId(panid);
  return kErrorNone;
}

extern "C" EmberEUI64 emLocalEui64;

Phy::Error Phy::SetExtendedAddress(uint8_t *address) {
  for (int i = 0; i < 8; i++)
    emLocalEui64[i] = address[i];
  emPhySetEui64();
  return kErrorNone;
}

Phy::Error Phy::SetShortAddress(uint16_t address) {
  emRadioSetNodeId(address);
 return kErrorNone;
}


void disable_interrupt() {
  INTERRUPTS_OFF(); 
}

void enable_interrupt() {
  INTERRUPTS_ON(); 
}

Phy::Error Phy::Start() {
  INTERRUPTS_ON();
  //CpuClock::enable(PERIDIS_ADC_BIT);

  emRadioInit(EMBER_RADIO_POWER_MODE_OFF);
  INT_CFGCLR = INT_MACRX;
  emRadioEnableAddressMatching(true);
  emRadioEnableAutoAck(true);
  emRadioEnablePacketTrace(false);

  emRadioWakeUp();
  INT_CFGCLR = INT_MACRX;
  emSetPhyRadioChannel(11);

  femCsd.init();
  femCps.init();
  femCtx.init();

  femCsd.hi();
  femCps.hi();
  femCtx.lo();

  state_ = kStateIdle;
  return kErrorNone;
}

Phy::Error Phy::Stop() {
  return kErrorNone;
}

Phy::Error Phy::Sleep() {
  Error error = kErrorNone;

  disable_interrupt();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateSleep;

  emRadioSleep();
exit:
  enable_interrupt();
  return error;
}

Phy::Error Phy::Idle() {
  Error error = kErrorNone;

  disable_interrupt();
  switch (state_) {
    case kStateSleep:
      emRadioWakeUp();
      INT_CFGCLR = INT_MACRX;
      state_ = kStateIdle;
      break;
    case kStateIdle:
      break;
    case kStateListen:
      INT_CFGCLR = INT_MACRX;
      state_ = kStateIdle;
      break;
    case kStateTransmit:
    case kStateDisabled:
    case kStateReceive:
      ExitNow(error = kErrorInvalidState);
      break;
  }

exit:
  enable_interrupt();
  return error;
}

Phy::Error Phy::Receive(PhyPacket *packet) {
  Error error = kErrorNone;

  disable_interrupt();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateListen;
  receive_packet_ = packet;

  if(emGetPhyRadioChannel() != packet->GetChannel())
    emSetPhyRadioChannel(packet->GetChannel());

  INT_CFGSET = INT_MACRX;

exit:
  enable_interrupt();
  return error;
}

Phy::Error Phy::Transmit(PhyPacket *packet) {
  Error error = kErrorNone;

  disable_interrupt();
  VerifyOrExit(state_ == kStateIdle, error = kErrorInvalidState);
  state_ = kStateTransmit;
  transmit_packet_ = packet;

  femCtx.hi();

  if(emGetPhyRadioChannel() != packet->GetChannel())
    emSetPhyRadioChannel(packet->GetChannel());

  if(emGetPhyRadioPower() != packet->GetPower())
    emSetPhyRadioPower(packet->GetPower());
    
  if(emRadioCheckRadio()) {
    emberCalibrateCurrentChannel();
  }

  INT_CFGCLR = INT_MACRX;
  emRadioTransmit(transmit_packet_->GetPsdu() - 1);
  INT_CFGCLR = INT_MACRX;

exit:
  enable_interrupt();
  return error;
} 
Phy::State Phy::GetState() {
  return state_;
}

int8_t Phy::GetNoiseFloor() {
  return 0;
}

void Phy::SentTask(void *context) {
  Phy *obj = reinterpret_cast<Phy*>(context);
  obj->SentTask();
}

void Phy::SentTask() {
  assert(state_ == kStateTransmit);
  state_ = kStateIdle;
  callbacks_->HandleTransmitDone(transmit_packet_, false, transmit_error_);
}

void Phy::ReceivedTask(void *context) {
  Phy *obj = reinterpret_cast<Phy*>(context);
  obj->ReceivedTask();
}

void Phy::ReceivedTask() {
  assert(state_ == kStateListen);
  state_ = kStateIdle;
  callbacks_->HandleReceiveDone(receive_packet_, receive_error_);
}

//extern "C" __WEAK void emberRadioMacTimerCompareIsrCallback() {}
//extern "C" __WEAK void emberRadioDataPendingLongIdIsrCallback() {}
//extern "C" __WEAK void emberRadioDataPendingShortIdIsrCallback() {}
//extern "C" __WEAK void emberRadioOverflowIsrCallback() {}
//extern "C" __WEAK void emberRadioSfdSentIsrCallback() {}

extern "C" void emberRadioReceiveIsrCallback(uint8_t *packet,
                                         bool ackFramePendingSet,
                                         uint32_t time,
                                         uint16_t errors,
                                         int8_t rssi){
  if(phy_){
    //Todo: translate emErrors to Thread Errors
    //*(uint8_t *)(&(phy_->receive_error_)) = errors;
    phy_->HandleReceivedFrame(packet);
  }
}


extern "C" void emberRadioTransmitCompleteIsrCallback(EmberStatus status,
                                                  int32u sfdSentTime,
                                                  boolean framePending)
{
  femCtx.lo();

  if(phy_) {
    //Todo: translate emErrors to Thread Errors
    //*(EmberStatus *)(&(phy_->transmit_error_)) = status;
    phy_->HandleTransmitEvent();
  }
}

extern "C" void emberRadioTxAckIsrCallback() {
//  phy_->HandleTransmitEvent();
}

//unused for now
void Phy::HandleReceiveEvent() {
    //HandleReceivedFrame(readBuf % 8);
}

void Phy::HandleReceivedFrame(uint8_t *packet) {

  if(receive_packet_){
    receive_packet_->SetPsduLength(packet[0]);
    memcpy(receive_packet_->GetPsdu(), packet + 1, receive_packet_->GetPsduLength());
  }
  received_task_.Post();
  INT_CFGCLR = INT_MACRX;
}

void Phy::HandleTransmitEvent() {
  sent_task_.Post();
  INT_CFGCLR = INT_MACRX;
}

}  // namespace Thread
