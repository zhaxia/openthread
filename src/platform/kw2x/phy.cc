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

#include <platform/common/alarm.h>
#include <platform/common/phy.h>
#include <common/code_utils.h>
#include <common/tasklet.h>
#include <mac/mac_frame.h>
#include <core/cpu.h>
#include <bsp/PLM/Interface/EmbeddedTypes.h>
#include <bsp/PLM/Source/Common/MC1324xDrv/MC1324xDrv.h>
#include <bsp/PLM/Source/Common/MC1324xDrv/MC1324xReg.h>
#include <bsp/MacPhy/Phy/Interface/Phy.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    // Time in milliseconds required by the transceiver to switch the
    // CLK_OUT clock frequency (in our case from 32KHz to 4 MHz)
    mClkSwitchDelayTime_c = 50,
    mRst_B_AssertTime_c = 50,

    // CLK_OUT_DIV field value for 4 MHz clock out frequency
    mCLK_OUT_DIV_4MHz_c = 3,
};

extern void phy_handle_transmit_done(PhyPacket *packet, bool rx_pending, ThreadError error);
extern void phy_handle_receive_done(PhyPacket *packet, ThreadError error);

static void received_task(void *context);
static void sent_task(void *context);
static Thread::Tasklet received_task_(&received_task, NULL);
static Thread::Tasklet sent_task_(&sent_task, NULL);

static PhyState s_state = kStateDisabled;
static PhyPacket *s_receive_frame = NULL;
static PhyPacket *s_transmit_frame = NULL;
static ThreadError s_transmit_error;
static ThreadError s_receive_error;
static phyRxParams_t s_rx_params;

static uint8_t s_phy_events[64];
static uint8_t s_phy_events_cur = 0;

static void delayMs(uint16_t val)
{
    uint32_t now = Thread::alarm_get_now();

    while (Thread::alarm_get_now() - now <= val) {}
}

static void phy_doze()
{
    uint8_t phyPwrModesReg = 0;
    phyPwrModesReg = MC1324xDrv_DirectAccessSPIRead(PWR_MODES);
    phyPwrModesReg |= (0x10);  /* XTALEN = 1 */
    phyPwrModesReg &= (0xFE);  /* PMC_MODE = 0 */
    MC1324xDrv_DirectAccessSPIWrite(PWR_MODES, phyPwrModesReg);
}

ThreadError phy_set_pan_id(uint16_t panid)
{
    uint8_t buf[2];
    buf[0] = panid >> 0;
    buf[1] = panid >> 8;
    PhyPpSetPanIdPAN0(buf);
    return kThreadError_None;
}

ThreadError phy_set_extended_address(uint8_t *address)
{
    PhyPpSetLongAddrPAN0(address);
    return kThreadError_None;
}

ThreadError phy_set_short_address(uint16_t address)
{
    uint8_t buf[2];
    buf[0] = address >> 0;
    buf[1] = address >> 8;
    PhyPpSetShortAddrPAN0(buf);
    return kThreadError_None;
}

ThreadError phy_init()
{
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
    phy_doze();

    return kThreadError_None;
}

ThreadError phy_start()
{
    ThreadError error = kThreadError_None;

    MC1324xDrv_IRQ_Disable();
    VerifyOrExit(s_state == kStateDisabled, error = kThreadError_Busy);
    s_state = kStateSleep;

exit:
    MC1324xDrv_IRQ_Enable();
    return error;
}

ThreadError phy_stop()
{
    MC1324xDrv_IRQ_Disable();
    PhyAbort();
    phy_doze();
    s_state = kStateDisabled;
    MC1324xDrv_IRQ_Enable();
    return kThreadError_None;
}

ThreadError phy_sleep()
{
    ThreadError error = kThreadError_None;

    MC1324xDrv_IRQ_Disable();
    VerifyOrExit(s_state == kStateIdle, error = kThreadError_Busy);
    phy_doze();
    s_state = kStateSleep;

exit:
    MC1324xDrv_IRQ_Enable();
    return error;
}

ThreadError phy_idle()
{
    ThreadError error = kThreadError_None;

    MC1324xDrv_IRQ_Disable();

    switch (s_state)
    {
    case kStateSleep:
        s_state = kStateIdle;
        break;

    case kStateIdle:
        break;

    case kStateListen:
    case kStateTransmit:
        PhyAbort();
        s_state = kStateIdle;
        break;

    case kStateDisabled:
    case kStateReceive:
        ExitNow(error = kThreadError_Busy);
        break;
    }

exit:
    MC1324xDrv_IRQ_Enable();
    return error;
}

ThreadError phy_receive(PhyPacket *packet)
{
    ThreadError error = kThreadError_None;

    MC1324xDrv_IRQ_Disable();
    VerifyOrExit(s_state == kStateIdle, error = kThreadError_Busy);
    s_state = kStateListen;
    s_receive_frame = packet;
    VerifyOrExit(PhyPlmeSetCurrentChannelRequestPAN0(packet->m_channel) == gPhySuccess_c, error = kThreadError_Busy);
    VerifyOrExit(PhyPlmeRxRequest((phyPacket_t *)s_receive_frame, 0, &s_rx_params) == gPhySuccess_c,
                 error = kThreadError_Busy);
    s_phy_events[s_phy_events_cur++] = 0x10;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

exit:
    MC1324xDrv_IRQ_Enable();
    return error;
}

uint8_t GetPhyTxMode(PhyPacket *packet)
{
    return reinterpret_cast<Thread::Mac::Frame *>(packet)->GetAckRequest() ?
           gDataReq_Ack_Cca_Unslotted_c : gDataReq_NoAck_Cca_Unslotted_c;
}

ThreadError phy_transmit(PhyPacket *packet)
{
    ThreadError error = kThreadError_None;

    MC1324xDrv_IRQ_Disable();
    VerifyOrExit(s_state == kStateIdle, error = kThreadError_Busy);
    s_state = kStateTransmit;
    s_transmit_frame = packet;
    VerifyOrExit(PhyPlmeSetCurrentChannelRequestPAN0(packet->m_channel) == gPhySuccess_c, error = kThreadError_Busy);
    VerifyOrExit(PhyPdDataRequest((phyPacket_t *)s_transmit_frame, GetPhyTxMode(s_transmit_frame), NULL) == gPhySuccess_c,
                 error = kThreadError_Busy);
    s_phy_events[s_phy_events_cur++] = 0x11;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

exit:
    assert(error == kThreadError_None);
    MC1324xDrv_IRQ_Enable();
    return error;
}

PhyState phy_get_state()
{
    return s_state;
}

int8_t phy_get_noise_floor()
{
    return 0;
}

extern "C" void PhyPlmeSyncLossIndication()
{
    s_phy_events[s_phy_events_cur++] = 1;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    switch (s_state)
    {
    case kStateDisabled:
    case kStateSleep:
        break;

    case kStateListen:
        s_receive_error = kThreadError_Abort;
        PhyAbort();
        s_state = kStateReceive;
        received_task_.Post();
        break;

    default:
        assert(false);
        break;
    }
}

extern "C" void PhyTimeRxTimeoutIndication()
{
    s_phy_events[s_phy_events_cur++] = 2;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    assert(false);
}

extern "C" void PhyTimeStartEventIndication()
{
    s_phy_events[s_phy_events_cur++] = 3;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    assert(false);
}

extern "C" void PhyPlmeCcaConfirm(bool_t channelInUse)
{
    s_phy_events[s_phy_events_cur++] = 4;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    switch (s_state)
    {
    case kStateDisabled:
    case kStateSleep:
        break;

    case kStateTransmit:
        s_transmit_error = kThreadError_Abort;
        sent_task_.Post();
        break;

    default:
        assert(false);
        break;
    }
}

extern "C" void PhyPlmeEdConfirm(uint8_t energyLevel)
{
    s_phy_events[s_phy_events_cur++] = 5;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    assert(false);
}

extern "C" void PhyPdDataConfirm()
{
    s_phy_events[s_phy_events_cur++] = 6;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    assert(s_state == kStateTransmit);
    s_transmit_error = kThreadError_None;
    sent_task_.Post();
}

extern "C" void PhyPdDataIndication()
{
    int rssi;

    s_phy_events[s_phy_events_cur++] = 7;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    switch (s_state)
    {
    case kStateDisabled:
    case kStateSleep:
        break;

    case kStateListen:
    case kStateReceive:
        s_state = kStateReceive;

        rssi = s_rx_params.linkQuality;
        rssi = ((rssi * 105) / 255) - 105;

        s_receive_frame->m_power = rssi;

        s_receive_error = kThreadError_None;
        received_task_.Post();
        break;

    default:
        assert(false);
    }
}

extern "C" void PhyPlmeFilterFailRx()
{
    s_phy_events[s_phy_events_cur++] = 8;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

#if 1

    switch (s_state)
    {
    case kStateDisabled:
    case kStateSleep:
        break;

    case kStateIdle:
        PhyAbort();
        break;

    case kStateListen:
    case kStateReceive:
        PhyAbort();
        PhyPlmeRxRequest((phyPacket_t *)s_receive_frame, 0, &s_rx_params);
        s_state = kStateListen;
        break;

    case kStateTransmit:
        break;

    default:
        assert(false);
        break;
    }

#else

    switch (s_state)
    {
    case kStateDisabled:
    case kStateSleep:
        break;

    case kStateListen:
        break;

    case kStateReceive:
        s_state = kStateListen;
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
    s_phy_events[s_phy_events_cur++] = 9;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    switch (s_state)
    {
    case kStateDisabled:
    case kStateSleep:
        break;

    case kStateListen:
        s_state = kStateReceive;
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
    s_phy_events[s_phy_events_cur++] = 10;

    if (s_phy_events_cur >= sizeof(s_phy_events))
    {
        s_phy_events_cur = 0;
    }

    assert(false);
}

void sent_task(void *context)
{
    VerifyOrExit(s_state != kStateDisabled, ;);
    assert(s_state == kStateTransmit);
    s_state = kStateIdle;
    phy_handle_transmit_done(s_transmit_frame, PhyPpIsRxAckDataPending(), s_transmit_error);

exit:
    {}
}

void received_task(void *context)
{
    VerifyOrExit(s_state != kStateDisabled, ;);
    assert(s_state == kStateReceive);
    s_state = kStateIdle;
    phy_handle_receive_done(s_receive_frame, s_receive_error);

exit:
    {}
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
