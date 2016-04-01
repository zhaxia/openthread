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

#include <platform/common/phy.h>
#include <common/code_utils.h>
#include <common/tasklet.h>
#include <mac/mac_frame.h>
#include <bsp/sdk/bsp/include/global_io.h>
#include <bsp/sdk/interfaces/ftdf/src/regmap.h>

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_rf.h"
#include "hw_fem_sky66112-11.h"

/// When flag is zero, LMAC HW will put transceiver into RX mode
/// when not transmitting.  When flag not defined, rx/tx toggling
/// is done manually via SW state transistions.
#define FEATURE_MAX_RX_WINDOW   1
#ifdef HW_NL_DA15100
#define FEATURE_FEM_DRIVER      1
#else
#define FEATURE_FEM_DRIVER      0
#endif
#define FEATURE_TEST_MAC_SEQ    0


#define RF_MODE_BLE             0

#define FTDF_CCA_MODE_1   1
#define FTDF_PHYTXSTARTUP 0x4c
#define FTDF_PHYTXLATENCY 0x01
#define FTDF_PHYTXFINISH  0x00
#define FTDF_PHYTRXWAIT   0x3f
//#define FTDF_PHYTRXWAIT   0x0f
#define FTDF_PHYRXSTARTUP 0
#define FTDF_PHYRXLATENCY 0
#define FTDF_PHYENABLE    0

#define FTDF_BUFFER_LENGTH         128
#define FTDF_TX_DATA_BUFFER        0
#define FTDF_TX_WAKEUP_BUFFER      1
#define FTDF_TX_ACK_BUFFER         2

#define FTDF_MSK_RX_CE             0x00000002
#define FTDF_MSK_SYMBOL_TMR_CE     0x00000008
#define FTDF_MSK_TX_CE             0x00000010

#define FTDF_GET_FIELD_ADDR(fieldName)                ((volatile uint32_t*) (IND_F_FTDF_ ## fieldName))
#define FTDF_GET_FIELD_ADDR_INDEXED(fieldName, index) ((volatile uint32_t*) (IND_F_FTDF_ ## fieldName + \
                                                                                 (intptr_t) index * \
                                                                                 FTDF_ ## fieldName ## _INTVL))
#define FTDF_GET_REG_ADDR(regName)                    ((volatile uint32_t*) (IND_R_FTDF_ ## regName))
#define FTDF_GET_REG_ADDR_INDEXED(regName, index)     ((volatile uint32_t*) (IND_R_FTDF_ ## regName + \
                                                                                 (intptr_t) index * \
                                                                                 FTDF_ ## regName ## _INTVL))
#define FTDF_GET_FIELD(fieldName)                     (((*(volatile uint32_t*) (IND_F_FTDF_ ## fieldName)) & \
                                                            MSK_F_FTDF_ ## fieldName) >> OFF_F_FTDF_ ## fieldName)
#define FTDF_GET_FIELD_INDEXED(fieldName, \
                                index)                 (((*FTDF_GET_FIELD_ADDR_INDEXED(fieldName, \
                                                                                            index)) \
                                                            & MSK_F_FTDF_ ## fieldName) >> OFF_F_FTDF_ ## fieldName)

#define FTDF_SET_FIELD(fieldName, value)              do {            \
    uint32_t tmp =  *FTDF_GET_FIELD_ADDR(fieldName) & ~MSK_F_FTDF_ ## fieldName; \
    *FTDF_GET_FIELD_ADDR(fieldName) = tmp | \
        (((value) << OFF_F_FTDF_ ## fieldName) & MSK_F_FTDF_ ## fieldName); \
  } \
  while (0)


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
    FTDF_SET_FIELD(ON_OFF_REGMAP_MACPANID, panid);
    return kThreadError_None;
}

ThreadError phy_set_extended_address(uint8_t *address)
{
    uint32_t extAddress[2];
    uint8_t *addr = reinterpret_cast<uint8_t *>(extAddress);

    for (int i = 0; i < 8; i++)
    {
        addr[i] = address[i];
    }

    FTDF_SET_FIELD(ON_OFF_REGMAP_AEXTENDEDADDRESS_L, extAddress[0]);
    FTDF_SET_FIELD(ON_OFF_REGMAP_AEXTENDEDADDRESS_H, extAddress[1]);
    return kThreadError_None;
}

ThreadError phy_set_short_address(uint16_t address)
{
    FTDF_SET_FIELD(ON_OFF_REGMAP_MACSHORTADDRESS, address);
    return kThreadError_None;
}


void disable_interrupt()
{
    NVIC_DisableIRQ(FTDF_GEN_IRQn);
}

void enable_interrupt()
{
    NVIC_ClearPendingIRQ(FTDF_GEN_IRQn);
    NVIC_EnableIRQ(FTDF_GEN_IRQn);
}

void phy_power_init()
{
    // Set the radio_ldo voltage to 1.4 Volts
    REG_SETF(CRG_TOP, LDO_CTRL1_REG, LDO_RADIO_SETVDD, 2);
    // Switch on the RF LDO
    REG_SETF(CRG_TOP, LDO_CTRL1_REG, LDO_RADIO_ENABLE, 1);
}

void ad_ftdf_init_phy_api(void)
{
    NVIC_ClearPendingIRQ(FTDF_WAKEUP_IRQn);
    NVIC_EnableIRQ(FTDF_WAKEUP_IRQn);

    NVIC_ClearPendingIRQ(FTDF_GEN_IRQn);
    NVIC_EnableIRQ(FTDF_GEN_IRQn);

    volatile uint32_t *lmacReset = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_LMACRESET);
    *lmacReset = MSK_R_FTDF_ON_OFF_REGMAP_LMACRESET;

    volatile uint32_t *controlStatus = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_LMAC_CONTROL_STATUS);

    while ((*controlStatus & MSK_F_FTDF_ON_OFF_REGMAP_LMACREADY4SLEEP) == 0) {}

    volatile uint32_t *wakeupTimerEnableStatus = FTDF_GET_FIELD_ADDR(ON_OFF_REGMAP_WAKEUPTIMERENABLESTATUS);
    FTDF_SET_FIELD(ALWAYS_ON_REGMAP_WAKEUPTIMERENABLE, 0);

    while (*wakeupTimerEnableStatus & MSK_F_FTDF_ON_OFF_REGMAP_WAKEUPTIMERENABLESTATUS) {}

    FTDF_SET_FIELD(ALWAYS_ON_REGMAP_WAKEUPTIMERENABLE, 1);

    while ((*wakeupTimerEnableStatus & MSK_F_FTDF_ON_OFF_REGMAP_WAKEUPTIMERENABLESTATUS) == 0) {}
}

void ad_ftdf_init_lmac()
{
    FTDF_SET_FIELD(ON_OFF_REGMAP_CCAIDLEWAIT, 192);
    volatile uint32_t *txFlagClear = FTDF_GET_FIELD_ADDR(ON_OFF_REGMAP_TX_FLAG_CLEAR);
    *txFlagClear = MSK_F_FTDF_ON_OFF_REGMAP_TX_FLAG_CLEAR;

    volatile uint32_t *phyParams = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_PHY_PARAMETERS_2);
    *phyParams = (FTDF_PHYTXSTARTUP << OFF_F_FTDF_ON_OFF_REGMAP_PHYTXSTARTUP) |
                 (FTDF_PHYTXLATENCY << OFF_F_FTDF_ON_OFF_REGMAP_PHYTXLATENCY) |
                 (FTDF_PHYTXFINISH << OFF_F_FTDF_ON_OFF_REGMAP_PHYTXFINISH) |
                 (FTDF_PHYTRXWAIT << OFF_F_FTDF_ON_OFF_REGMAP_PHYTRXWAIT);

    phyParams = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_PHY_PARAMETERS_3);
    *phyParams = (FTDF_PHYRXSTARTUP << OFF_F_FTDF_ON_OFF_REGMAP_PHYRXSTARTUP) |
                 (FTDF_PHYRXLATENCY << OFF_F_FTDF_ON_OFF_REGMAP_PHYRXLATENCY) |
                 (FTDF_PHYENABLE << OFF_F_FTDF_ON_OFF_REGMAP_PHYENABLE);

    volatile uint32_t *ftdfCm = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_FTDF_CM);
    *ftdfCm = FTDF_MSK_TX_CE | FTDF_MSK_RX_CE | FTDF_MSK_SYMBOL_TMR_CE;

    volatile uint32_t *rxMask = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_RX_MASK);
    *rxMask = MSK_R_FTDF_ON_OFF_REGMAP_RX_MASK;

    volatile uint32_t *lmacMask = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_LMAC_MASK);
    *lmacMask = MSK_F_FTDF_ON_OFF_REGMAP_RXTIMEREXPIRED_M;

    volatile uint32_t *lmacCtrlMask = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_LMAC_CONTROL_MASK);
    *lmacCtrlMask = MSK_F_FTDF_ON_OFF_REGMAP_SYMBOLTIMETHR_M |
                    MSK_F_FTDF_ON_OFF_REGMAP_SYMBOLTIME2THR_M |
                    MSK_F_FTDF_ON_OFF_REGMAP_SYNCTIMESTAMP_M;

    volatile uint32_t *txFlagClearM;
    txFlagClearM   = FTDF_GET_FIELD_ADDR_INDEXED(ON_OFF_REGMAP_TX_FLAG_CLEAR_M, FTDF_TX_DATA_BUFFER);
    *txFlagClearM |= MSK_F_FTDF_ON_OFF_REGMAP_TX_FLAG_CLEAR_M;
    txFlagClearM   = FTDF_GET_FIELD_ADDR_INDEXED(ON_OFF_REGMAP_TX_FLAG_CLEAR_M, FTDF_TX_WAKEUP_BUFFER);
    *txFlagClearM |= MSK_F_FTDF_ON_OFF_REGMAP_TX_FLAG_CLEAR_M;
}


/// void ad_ftdf_init(void)
ThreadError phy_init()
{
    /* Wake up power domains */
    REG_CLR_BIT(CRG_TOP, PMU_CTRL_REG, FTDF_SLEEP);

    while (REG_GETF(CRG_TOP, SYS_STAT_REG, FTDF_IS_UP) == 0x0);

    REG_CLR_BIT(CRG_TOP, PMU_CTRL_REG, RADIO_SLEEP);

    while (REG_GETF(CRG_TOP, SYS_STAT_REG, RAD_IS_UP) == 0x0);

    REG_SETF(CRG_TOP, CLK_RADIO_REG, FTDF_MAC_ENABLE, 1);
    REG_SETF(CRG_TOP, CLK_RADIO_REG, FTDF_MAC_DIV, 0);

    phy_power_init();
    hw_rf_system_init(RF_MODE_BLE);
    hw_rf_set_recommended_settings(RF_MODE_BLE);
    hw_rf_iff_calibration();

    hw_rf_modulation_gain_calibration(RF_MODE_BLE);
    hw_rf_dc_offset_calibration();

    ad_ftdf_init_phy_api();
    ad_ftdf_init_lmac();

#if FEATURE_FEM_DRIVER
    hw_fem_start();
#endif

    return kThreadError_None;
}

ThreadError phy_start()
{
    phy_init();
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

    REG_SETF(CRG_TOP, PMU_CTRL_REG, RADIO_SLEEP, 1);

    while ((CRG_TOP->SYS_STAT_REG | CRG_TOP_SYS_STAT_REG_RAD_IS_DOWN_Msk) == 0x0) {}

    REG_SETF(CRG_TOP, PMU_CTRL_REG, FTDF_SLEEP, 1);

    while ((CRG_TOP->SYS_STAT_REG | CRG_TOP_SYS_STAT_REG_FTDF_IS_DOWN_Msk) == 0x0) {}

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
        /* Wake up power domains */
        REG_SETF(CRG_TOP, PMU_CTRL_REG, FTDF_SLEEP, 0);

        while ((CRG_TOP->SYS_STAT_REG | CRG_TOP_SYS_STAT_REG_FTDF_IS_UP_Msk) == 0x0) {}

        REG_SETF(CRG_TOP, PMU_CTRL_REG, RADIO_SLEEP, 0);

        while ((CRG_TOP->SYS_STAT_REG | CRG_TOP_SYS_STAT_REG_RAD_IS_UP_Msk) == 0x0) {}

        hw_rf_system_init(RF_MODE_BLE);
        hw_rf_set_recommended_settings(RF_MODE_BLE);
        ad_ftdf_init_lmac();
        state_ = kStateIdle;
        break;

    case kStateIdle:
        break;

    case kStateListen:
#if FEATURE_MAX_RX_WINDOW
        FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 0);
        FTDF_SET_FIELD(ON_OFF_REGMAP_RXALWAYSON, 0);
        FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 1);
#endif //FEATURE_MAX_RX_WINDOW
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

#if FEATURE_MAX_RX_WINDOW
    disable_interrupt();
#endif //FEATURE_MAX_RX_WINDOW
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateListen;
    receive_packet_ = packet;

#if FEATURE_MAX_RX_WINDOW
    FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 0);
#endif //FEATURE_MAX_RX_WINDOW

    uint32_t phyAckAttr;
    phyAckAttr = 0x08 | ((packet->m_channel - 11) & 0xf) << 4 | (0 & 0x3) << 8;
    FTDF_SET_FIELD(ON_OFF_REGMAP_PHYRXATTR, (((packet->m_channel - 11) & 0xf) << 4));
    FTDF_SET_FIELD(ON_OFF_REGMAP_PHYACKATTR, phyAckAttr);

#if FEATURE_MAX_RX_WINDOW
    int writeBuf;
    writeBuf = FTDF_GET_FIELD(ON_OFF_REGMAP_RX_WRITE_BUF_PTR);
    FTDF_SET_FIELD(ON_OFF_REGMAP_RX_READ_BUF_PTR, writeBuf);
#endif //FEATURE_MAX_RX_WINDOW

    FTDF_SET_FIELD(ON_OFF_REGMAP_RXALWAYSON, 1);
    FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 1);

exit:
#if FEATURE_MAX_RX_WINDOW
    enable_interrupt();
#endif //FEATURE_MAX_RX_WINDOW

    return error;
}

ThreadError phy_transmit(PhyPacket *packet)
{
    ThreadError error = kThreadError_None;

#if FEATURE_MAX_RX_WINDOW
    disable_interrupt();
#endif //FEATURE_MAX_RX_WINDOW
    VerifyOrExit(state_ == kStateIdle, error = kThreadError_Busy);
    state_ = kStateTransmit;
    transmit_packet_ = packet;

#if FEATURE_MAX_RX_WINDOW
    FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 0);
    int writeBuf;
    writeBuf = FTDF_GET_FIELD(ON_OFF_REGMAP_RX_WRITE_BUF_PTR);
    FTDF_SET_FIELD(ON_OFF_REGMAP_RX_READ_BUF_PTR, writeBuf);
    FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 1);
#endif //FEATURE_MAX_RX_WINDOW

    {
        volatile uint8_t *buf = reinterpret_cast<volatile uint8_t *>(FTDF_GET_REG_ADDR(RETENTION_RAM_TX_FIFO) +
                                                                     (FTDF_BUFFER_LENGTH * FTDF_TX_DATA_BUFFER));
        uint8_t channel = packet->m_channel;
        uint8_t phyPayloadSize = packet->m_length;
        uint8_t *cur = packet->m_psdu;
        uint8_t frameType = cur[0] & 0x7;

        *buf++ = phyPayloadSize;

        for (int i = 0; i < packet->m_length; i++)
        {
            *buf++ = *cur++;
        }

        uint16_t phyAttr = (FTDF_CCA_MODE_1 & 0x3) | 0x08 | ((channel - 11) & 0x0F) << 4 | (0 & 0x03) << 8;

        volatile uint32_t *metaData0 = FTDF_GET_REG_ADDR_INDEXED(RETENTION_RAM_TX_META_DATA_0, FTDF_TX_DATA_BUFFER);
        volatile uint32_t *metaData1 = FTDF_GET_REG_ADDR_INDEXED(RETENTION_RAM_TX_META_DATA_1, FTDF_TX_DATA_BUFFER);
        *metaData0 =
            ((phyPayloadSize << OFF_F_FTDF_RETENTION_RAM_FRAME_LENGTH) & MSK_F_FTDF_RETENTION_RAM_FRAME_LENGTH) |
            ((phyAttr << OFF_F_FTDF_RETENTION_RAM_PHYATTR) & MSK_F_FTDF_RETENTION_RAM_PHYATTR) |
            ((frameType << OFF_F_FTDF_RETENTION_RAM_FRAMETYPE) & MSK_F_FTDF_RETENTION_RAM_FRAMETYPE) |
            (MSK_F_FTDF_RETENTION_RAM_CSMACA_ENA) |
            (reinterpret_cast<Mac::Frame *>(packet)->GetAckRequest() ? MSK_F_FTDF_RETENTION_RAM_ACKREQUEST : 0) |
            (MSK_F_FTDF_RETENTION_RAM_CRC16_ENA);

        uint8_t sequence;
        reinterpret_cast<Mac::Frame *>(packet)->GetSequence(sequence);
        *metaData1 = ((sequence << OFF_F_FTDF_RETENTION_RAM_MACSN) & MSK_F_FTDF_RETENTION_RAM_MACSN);

        uint32_t phyCsmaCaAttr = (FTDF_CCA_MODE_1 & 0x3) | ((channel - 11) & 0xf) << 4;
        FTDF_SET_FIELD(ON_OFF_REGMAP_PHYCSMACAATTR, phyCsmaCaAttr);

        volatile uint32_t *txFlagSet = FTDF_GET_FIELD_ADDR(ON_OFF_REGMAP_TX_FLAG_SET);
        *txFlagSet |= (1 << FTDF_TX_DATA_BUFFER);
    }

exit:
#if FEATURE_MAX_RX_WINDOW
    enable_interrupt();
#endif //FEATURE_MAX_RX_WINDOW
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

void phy_handle_received_frame(int read_buf)
{
    uint8_t *buf = reinterpret_cast<uint8_t *>(IND_R_FTDF_RX_RAM_RX_FIFO + (read_buf * FTDF_BUFFER_LENGTH));

    if (state_ == kStateTransmit)
    {
#if FEATURE_TEST_MAC_SEQ

        if (buf[0] == 5 && (buf[1] & Mac::Frame::kFcfFrameTypeMask) == Mac::Frame::kFcfFrameAck)
        {
            uint8_t dsn;
            reinterpret_cast<Mac::Frame *>(transmit_packet_)->GetSequence(dsn);

            if (buf[3] == dsn)
            {
                assert(false);
            }
        }

#endif
        return;
    }

    receive_packet_->m_length = buf[0];
    memcpy(receive_packet_->m_psdu, buf + 1, receive_packet_->m_length);

    received_task_.Post();
}

void handle_receive_event()
{
    volatile uint32_t *rxEvent = (volatile uint32_t *) IND_R_FTDF_ON_OFF_REGMAP_RX_EVENT;

    /* XXX TODO : Check if this is needed to fire an interrupt */
    if (*rxEvent & MSK_F_FTDF_ON_OFF_REGMAP_RXSOF_E)
    {
        *rxEvent  = MSK_F_FTDF_ON_OFF_REGMAP_RXSOF_E;
    }

    /* XXX TODO : Check if this is needed to fire an interrupt */
    if (*rxEvent & MSK_F_FTDF_ON_OFF_REGMAP_RXBYTE_E)
    {
        *rxEvent  = MSK_F_FTDF_ON_OFF_REGMAP_RXBYTE_E;
    }

    /* XXX TODO : Check if this is needed to fire an interrupt */
    if (*rxEvent & MSK_F_FTDF_ON_OFF_REGMAP_RX_OVERFLOW_E)
    {
        // No API defined to report this error to the higher layer, so just clear it.
        *rxEvent  = MSK_F_FTDF_ON_OFF_REGMAP_RX_OVERFLOW_E;
    }

    if (*rxEvent & MSK_F_FTDF_ON_OFF_REGMAP_RX_BUF_AVAIL_E)
    {
        int readBuf  = FTDF_GET_FIELD(ON_OFF_REGMAP_RX_READ_BUF_PTR);
        int writeBuf = FTDF_GET_FIELD(ON_OFF_REGMAP_RX_WRITE_BUF_PTR);

        while (readBuf != writeBuf)
        {
            phy_handle_received_frame(readBuf % 8);
            readBuf = (readBuf + 1) % 16;
        }

#if FEATURE_MAX_RX_WINDOW
        FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 0);
        FTDF_SET_FIELD(ON_OFF_REGMAP_RXALWAYSON, 0);
        FTDF_SET_FIELD(ON_OFF_REGMAP_RX_READ_BUF_PTR, readBuf);
        FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 1);
#else
        FTDF_SET_FIELD(ON_OFF_REGMAP_RX_READ_BUF_PTR, readBuf);
#endif //FEATURE_MAX_RX_WINDOW

        *rxEvent  = MSK_F_FTDF_ON_OFF_REGMAP_RX_BUF_AVAIL_E;
    }

    volatile uint32_t *lmacEvent = (volatile uint32_t *) IND_R_FTDF_ON_OFF_REGMAP_LMAC_EVENT;

    if (*lmacEvent & MSK_F_FTDF_ON_OFF_REGMAP_EDSCANREADY_E)
    {
        *lmacEvent  = MSK_F_FTDF_ON_OFF_REGMAP_EDSCANREADY_E;
    }

    if (*lmacEvent & MSK_F_FTDF_ON_OFF_REGMAP_RXTIMEREXPIRED_E)
    {
        *lmacEvent  = MSK_F_FTDF_ON_OFF_REGMAP_RXTIMEREXPIRED_E;
    }
}

void handle_transmit_event()
{
    volatile uint32_t *txFlagStatE;

#if FEATURE_MAX_RX_WINDOW
    FTDF_SET_FIELD(ON_OFF_REGMAP_RXENABLE, 0);
#endif //FEATURE_MAX_RX_WINDOW

    txFlagStatE = FTDF_GET_FIELD_ADDR_INDEXED(ON_OFF_REGMAP_TX_FLAG_CLEAR_E, FTDF_TX_DATA_BUFFER);

    if (*txFlagStatE & MSK_F_FTDF_ON_OFF_REGMAP_TX_FLAG_CLEAR_E)
    {
        *txFlagStatE = MSK_F_FTDF_ON_OFF_REGMAP_TX_FLAG_CLEAR_E;
    }
    else
    {
        return;
    }

    volatile uint32_t *txStatus = FTDF_GET_REG_ADDR_INDEXED(RETENTION_RAM_TX_RETURN_STATUS_1, FTDF_TX_DATA_BUFFER);

    if (*txStatus & MSK_F_FTDF_RETENTION_RAM_ACKFAIL)
    {
        // no ack // TODO: pass up kErrorNoAck to allow retries
        transmit_error_ = kThreadError_None;
    }
    else if (*txStatus & MSK_F_FTDF_RETENTION_RAM_CSMACAFAIL)
    {
        // no clear channel
        transmit_error_ = kThreadError_Abort;
    }
    else
    {
        transmit_error_ = kThreadError_None;
    }

    sent_task_.Post();
}

extern "C" void FTDF_GEN_Handler(void)
{
    volatile uint32_t ftdfCe = *FTDF_GET_REG_ADDR(ON_OFF_REGMAP_FTDF_CE);

    if (ftdfCe & FTDF_MSK_RX_CE)
    {
        handle_receive_event();
    }

    if (ftdfCe & FTDF_MSK_TX_CE)
    {
        handle_transmit_event();
    }

    if (ftdfCe & FTDF_MSK_SYMBOL_TMR_CE)
    {
        assert(false);
    }

    volatile uint32_t *ftdfCm = FTDF_GET_REG_ADDR(ON_OFF_REGMAP_FTDF_CM);
    *ftdfCm = FTDF_MSK_TX_CE | FTDF_MSK_RX_CE | FTDF_MSK_SYMBOL_TMR_CE;
}

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread
