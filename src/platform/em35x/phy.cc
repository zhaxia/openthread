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

#include <platform/common/phy.h>
#include <common/code_utils.h>
#include <common/tasklet.h>
#include <mac/mac_frame.h>
#include <cpu/CpuGpio.hpp>
#include <common/random.h>

#define PHY_EM3XX
#define CORTEXM3
#define CORTEXM3_EMBER_MICRO

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp/phy/ember-shim.h"
#include "bsp/phy/phy.h"

extern EmberStatus emApiSetTxPowerMode(uint8_t);
extern EmberStatus emRadioCheckRadio();
extern EmberStatus emberCalibrateCurrentChannel();
extern void calStartAdcConversion(uint8_t, uint8_t);
extern uint16_t calReadAdcBlocking();
extern void calDisableAdc();

nl::Thread::CpuGpio femCsd(0, 0, 1);
nl::Thread::CpuGpio femCps(1, 5, 1);
nl::Thread::CpuGpio femCtx(2, 5, 1);

extern void phy_handle_transmit_done(PhyPacket *packet, bool rx_pending, ThreadError error);
extern void phy_handle_receive_done(PhyPacket *packet, ThreadError error);

uint8_t phy_events_[64];
uint8_t phy_events_cur_ = 0;

PhyState state_ = kStateDisabled;
PhyPacket *receive_packet_ = NULL;
PhyPacket *transmit_packet_ = NULL;
ThreadError transmit_error_ = kThreadError_None;
ThreadError receive_error_ = kThreadError_None;

static void phy_received_task(void *context);
static void phy_sent_task(void *context);

Tasklet received_task_(&phy_received_task, NULL);
Tasklet sent_task_(&phy_sent_task, NULL);

ThreadError phy_set_pan_id(uint16_t panid)
{
    emRadioSetPanId(panid);
    return kThreadError_None;
}

extern "C" EmberEUI64 emLocalEui64;

ThreadError phy_set_extended_address(uint8_t *address)
{
    for (int i = 0; i < 8; i++)
    {
        emLocalEui64[i] = address[i];
    }

    emPhySetEui64();
    return kThreadError_None;
}

ThreadError phy_set_short_address(uint16_t address)
{
    emRadioSetNodeId(address);
    return kThreadError_None;
}


void disable_interrupt()
{
    INTERRUPTS_OFF();
}

void enable_interrupt()
{
    INTERRUPTS_ON();
}

ThreadError phy_init()
{
    calStartAdcConversion(CAL_ADC_CHANNEL_GND, ADC_SAMPLE_CLOCKS_32);

    for (int i = 0; i < 10; i++)
    {
        Random::Init(Random::Get() ^ calReadAdcBlocking());
    }

    calDisableAdc();
    return kThreadError_None;
}

ThreadError phy_start()
{
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
    return kThreadError_None;
}

ThreadError phy_stop()
{
    return kThreadError_None;
}

ThreadError phy_sleep()
{
    ThreadError error = kThreadError_None;

    disable_interrupt();
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateSleep;

    emRadioSleep();
exit:
    enable_interrupt();
    return error;
}

ThreadError phy_idle()
{
    ThreadError error = kThreadError_None;

    disable_interrupt();

    switch (state_)
    {
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
        ExitNow(error = kThreadError_Busy);
        break;
    }

exit:
    enable_interrupt();
    return error;
}

ThreadError phy_receive(PhyPacket *packet)
{
    ThreadError error = kThreadError_None;

    disable_interrupt();
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateListen;
    receive_packet_ = packet;

    if (emGetPhyRadioChannel() != packet->m_channel)
    {
        emSetPhyRadioChannel(packet->m_channel);
    }

    INT_CFGSET = INT_MACRX;

exit:
    enable_interrupt();
    return error;
}

ThreadError phy_transmit(PhyPacket *packet)
{
    ThreadError error = kThreadError_None;

    disable_interrupt();
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateTransmit;
    transmit_packet_ = packet;

    femCtx.hi();

    if (emGetPhyRadioChannel() != packet->m_channel)
    {
        emSetPhyRadioChannel(packet->m_channel);
    }

    if (emGetPhyRadioPower() != packet->m_power)
    {
        emSetPhyRadioPower(packet->m_power);
    }

    if (emRadioCheckRadio())
    {
        emberCalibrateCurrentChannel();
    }

    INT_CFGCLR = INT_MACRX;
    emRadioTransmit(transmit_packet_->m_psdu - 1);
    INT_CFGCLR = INT_MACRX;

exit:
    enable_interrupt();
    return error;
}

PhyState phy_get_state()
{
    return state_;
}

int8_t phy_get_noise_floor()
{
    return 0;
}

void phy_sent_task(void *context)
{
    assert(state_ == kStateTransmit);
    state_ = kStateIdle;
    phy_handle_transmit_done(transmit_packet_, false, transmit_error_);
}

void phy_received_task(void *context)
{
    assert(state_ == kStateListen);
    state_ = kStateIdle;
    phy_handle_receive_done(receive_packet_, receive_error_);
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
                                             int8_t rssi)
{
    if (receive_packet_)
    {
        receive_packet_->m_length = packet[0];
        memcpy(receive_packet_->m_psdu, packet + 1, receive_packet_->m_length);
    }

    received_task_.Post();
    INT_CFGCLR = INT_MACRX;
}


extern "C" void emberRadioTransmitCompleteIsrCallback(EmberStatus status,
                                                      int32u sfdSentTime,
                                                      boolean framePending)
{
    femCtx.lo();
    sent_task_.Post();
    INT_CFGCLR = INT_MACRX;
}

extern "C" void emberRadioTxAckIsrCallback()
{
}

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread
