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
 *    @author  Martin Turon <mturon@nestlabs.com>
 *
 */

#include <platform/common/phy.h>
#include <common/code_utils.h>
#include <common/tasklet.h>
#include <mac/mac_frame.h>

#include <bsp/ConnSW/ieee_802_15_4/Source/Phy/Interface/Phy.h>

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

// =======================================
//  KW4X PHY specific
// =======================================
void UnprotectFromXcvrInterrupt(void);
void ProtectFromXcvrInterrupt(void);
Phy_PhyLocalStruct_t            s_phyLocal;
phyRxParams_t                   s_rx_params;

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

// =======================================
//  Thread PHY API
// =======================================

uint8_t phy_events_[64];
uint8_t phy_events_cur_ = 0;

extern void phy_handle_transmit_done(PhyPacket *packet, bool rx_pending, ThreadError error);
extern void phy_handle_receive_done(PhyPacket *packet, ThreadError error);

static void phy_received_task(void *context);
static void phy_sent_task(void *context);

Tasklet received_task_(&phy_received_task, NULL);
Tasklet sent_task_(&phy_sent_task, NULL);

PhyState state_ = kStateDisabled;
PhyPacket *receive_packet_ = NULL;
PhyPacket *transmit_packet_ = NULL;
ThreadError transmit_error_ = kThreadError_None;
ThreadError receive_error_ = kThreadError_None;


ThreadError phy_set_pan_id(uint16_t panid)
{
    uint8_t buf[2];
    buf[0] = panid >> 0;
    buf[1] = panid >> 8;
    PhyPpSetPanId(buf, 0);
    return kThreadError_None;
}

ThreadError phy_set_extended_address(uint8_t *address)
{
    PhyPpSetLongAddr(address, 0);
    return kThreadError_None;
}

ThreadError phy_set_short_address(uint16_t address)
{
    uint8_t buf[2];
    buf[0] = address >> 0;
    buf[1] = address >> 8;
    PhyPpSetShortAddr(buf, 0);
    return kThreadError_None;
}

ThreadError phy_init()
{
    Phy_Init();
    return kThreadError_None;
}

ThreadError phy_start()
{
    phy_init();

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

    return kThreadError_None;
}

ThreadError phy_stop()
{
    ProtectFromXcvrInterrupt();
    //MC1324xDrv_RST_B_Assert();
    state_ = kStateDisabled;
    return kThreadError_None;
}

ThreadError phy_sleep()
{
    ThreadError error = kThreadError_None;

    ProtectFromXcvrInterrupt();
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateSleep;

exit:
    UnprotectFromXcvrInterrupt();
    return error;
}

ThreadError phy_idle()
{
    ThreadError error = kThreadError_None;

    ProtectFromXcvrInterrupt();

    switch (state_)
    {
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
        ExitNow(error = kThreadError_Busy);
        break;
    }

exit:
    UnprotectFromXcvrInterrupt();
    return error;
}

ThreadError phy_receive(PhyPacket *packet)
{
    ThreadError error = kThreadError_None;

    ProtectFromXcvrInterrupt();
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateListen;
    receive_packet_ = packet;
    VerifyOrExit(PhyPlmeSetCurrentChannelRequest(packet->m_channel, 0) == gPhySuccess_c,
                 error = kThreadError_Busy);
    // TODO: strap in rxBuffer
    // receive_packet_->m_psdu, 0,
    s_phyLocal.rxParams.duration = 0xFFFFFFFF;
    s_phyLocal.rxParams.phyRxMode = gPhyUnslottedMode_c;
    s_phyLocal.rxParams.pRxData =
        (pdDataToMacMessage_t *)receive_packet_->m_psdu;
    //PhyPlmeRxRequest(gPhyUnslottedMode_c, (phyRxParams_t*)&s_phyLocal.rxParams);
    VerifyOrExit(PhyPlmeRxRequest(gPhyUnslottedMode_c,
                                  &s_rx_params) == gPhySuccess_c,
                 error = kThreadError_Busy);
    phy_events_[phy_events_cur_++] = 0x10;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

exit:
    UnprotectFromXcvrInterrupt();
    return error;
}

phyTxParams_t GetPhyTxMode(PhyPacket *packet)
{
    //phyPacket_t *phyPacket = (phyPacket_t *)packet;
    phyTxParams_t txParams =
    {
        .numOfCca = (uint8_t)1, //(phyPacket->CCABeforeTx) ? (uint8_t)0 : (uint8_t)1,
        .ackRequired = (phyAckRequired_t)0  //phyPacket->ackRequired
    };
    return txParams;
}

ThreadError phy_transmit(PhyPacket *packet)
{
    ThreadError error = kThreadError_None;
    phyTxParams_t txParams = GetPhyTxMode(packet);

    ProtectFromXcvrInterrupt();
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateTransmit;
    transmit_packet_ = packet;
    VerifyOrExit(PhyPlmeSetCurrentChannelRequest(packet->m_channel, 0)
                 == gPhySuccess_c, error = kThreadError_Busy);

    VerifyOrExit(PhyPdDataRequest((pdDataReq_t *)packet->m_psdu, NULL, &txParams)
                 == gPhySuccess_c, error = kThreadError_Busy);


    phy_events_[phy_events_cur_++] = 0x11;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

exit:
    assert(error == kThreadError_None);
    UnprotectFromXcvrInterrupt();
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






extern "C" void PhyPlmeSyncLossIndication()
{
    phy_events_[phy_events_cur_++] = 1;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    switch (state_)
    {
    case kStateListen:
        receive_error_ = kThreadError_Abort;
        PhyAbort();
        state_ = kStateReceive;
        received_task_.Post();
        break;

    default:
        assert(false);
        break;
    }
}

extern "C" void PhyTimeRxTimeoutIndication()
{
    phy_events_[phy_events_cur_++] = 2;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    assert(false);
}

extern "C" void PhyTimeStartEventIndication()
{
    phy_events_[phy_events_cur_++] = 3;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    assert(false);
}

extern "C" void PhyPlmeCcaConfirm(bool_t channelInUse)
{
    phy_events_[phy_events_cur_++] = 4;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    switch (state_)
    {
    case kStateTransmit:
        transmit_error_ = kThreadError_Abort;
        sent_task_.Post();
        break;

    default:
        assert(false);
        break;
    }
}

extern "C" void PhyPlmeEdConfirm(uint8_t energyLevel)
{
    phy_events_[phy_events_cur_++] = 5;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    assert(false);
}

extern "C" void PhyPdDataConfirm()
{
    phy_events_[phy_events_cur_++] = 6;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    assert(state_ == kStateTransmit);
    transmit_error_ = kThreadError_None;
    sent_task_.Post();
}

extern "C" void PhyPdDataIndication()
{
    phy_events_[phy_events_cur_++] = 7;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    assert(state_ == kStateListen || state_ == kStateReceive);
    state_ = kStateReceive;

    int rssi;
    rssi = s_rx_params.linkQuality;
    rssi = ((rssi * 105) / 255) - 105;

    receive_packet_->m_power = rssi;

    receive_error_ = kThreadError_None;
    received_task_.Post();
}

extern "C" void PhyPlmeFilterFailRx()
{
    phy_events_[phy_events_cur_++] = 8;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

#if 1

    switch (state_)
    {
    case kStateListen:
    case kStateReceive:
        PhyAbort();
        // TODO: strap in rxBuffer
        // receive_packet_->m_psdu, 0,
        PhyPlmeRxRequest(gPhyUnslottedMode_c, &s_rx_params);
        state_ = kStateListen;
        break;

    case kStateTransmit:
        break;

    default:
        assert(false);
        break;
    }

#else

    switch (state_)
    {
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

extern "C" void PhyPlmeRxSfdDetect(uint8_t frame_length)
{
    phy_events_[phy_events_cur_++] = 9;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    //static uint8_t framelen_ = PhyPpGetState();
    switch (state_)
    {
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

extern "C" void PhyUnexpectedTransceiverReset()
{
    phy_events_[phy_events_cur_++] = 10;

    if (phy_events_cur_ >= sizeof(phy_events_))
    {
        phy_events_cur_ = 0;
    }

    while (1) {}
}


#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread
