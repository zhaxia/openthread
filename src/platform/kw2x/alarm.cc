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
#include <platform/common/atomic.h>
#include <common/timer.h>
#include <bsp/PLM/Interface/EmbeddedTypes.h>
#include <bsp/PLM/Source/Common/Interface/MK21D5.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DELAY 32768

extern void alarm_fired();

static void set_alarm();
static void set_hardware_alarm(uint16_t t0, uint16_t dt);

static uint16_t s_timer_hi = 0;
static uint16_t s_timer_lo = 0;
static uint32_t s_alarm_t0 = 0;
static uint32_t s_alarm_dt = 0;
static bool s_is_running = false;

void alarm_init()
{
    /* turn on the LPTMR clock */
    SIM_SCGC5 |= SIM_SCGC5_LPTIMER_MASK;
    /* disable LPTMR */
    LPTMR0_CSR &= ~LPTMR_CSR_TEN_MASK;
    /* 1ms tick period */
    LPTMR0_PSR = (LPTMR_PSR_PBYP_MASK | LPTMR_PSR_PCS(1));
    /* enable LPTMR IRQ */
    NVICICPR1 = 1 << 26;
    NVICISER1 = 1 << 26;

    LPTMR0_CMR = MAX_DELAY;

    s_timer_hi = 0;
    s_timer_lo = 0;
    s_is_running = false;

    /* enable LPTMR */
    LPTMR0_CSR = LPTMR_CSR_TCF_MASK | LPTMR_CSR_TIE_MASK | LPTMR_CSR_TFC_MASK;
    LPTMR0_CSR |= LPTMR_CSR_TEN_MASK;
}

uint32_t alarm_get_now()
{
    uint32_t int_state = atomic_begin();
    uint16_t timer_lo;

    LPTMR0_CNR = LPTMR0_CNR;
    timer_lo = LPTMR0_CNR;

    if (timer_lo < s_timer_lo)
    {
        s_timer_hi++;
    }

    s_timer_lo = timer_lo;
    atomic_end(int_state);

    return (static_cast<uint32_t>(s_timer_hi) << 16) | s_timer_lo;
}

void set_alarm()
{
    uint32_t now = alarm_get_now();
    uint32_t remaining = MAX_DELAY;
    uint32_t expires;

    if (s_is_running)
    {
        expires = s_alarm_t0 + s_alarm_dt;
        remaining = expires - now;

        if (s_alarm_t0 <= now)
        {
            if (expires >= s_alarm_t0 && expires <= now)
            {
                remaining = 0;
            }
        }
        else
        {
            if (expires >= s_alarm_t0 || expires <= now)
            {
                remaining = 0;
            }
        }

        if (remaining > MAX_DELAY)
        {
            s_alarm_t0 = now + MAX_DELAY;
            s_alarm_dt = remaining - MAX_DELAY;
            remaining = MAX_DELAY;
        }
        else
        {
            s_alarm_t0 += s_alarm_dt;
            s_alarm_dt = 0;
        }
    }

    set_hardware_alarm(now, remaining);
}

void set_hardware_alarm(uint16_t t0, uint16_t dt)
{
    uint32_t int_state = atomic_begin();
    uint16_t now, elapsed;
    uint16_t remaining;

    LPTMR0_CNR = LPTMR0_CNR;
    now = LPTMR0_CNR;
    elapsed = now - t0;

    if (elapsed >= dt)
    {
        LPTMR0_CMR = now + 2;
    }
    else
    {
        remaining = dt - elapsed;

        if (remaining <= 2)
        {
            LPTMR0_CMR = now + 2;
        }
        else
        {
            LPTMR0_CMR = now + remaining;
        }
    }

    LPTMR0_CSR |= LPTMR_CSR_TCF_MASK;
    atomic_end(int_state);
}

void alarm_start_at(uint32_t t0, uint32_t dt)
{
    uint32_t int_state = atomic_begin();

    s_alarm_t0 = t0;
    s_alarm_dt = dt;
    s_is_running = true;
    set_alarm();
    atomic_end(int_state);
}

void alarm_stop()
{
    if (s_is_running)
    {
        s_is_running = false;
        set_alarm();
    }
}

extern "C" void LPTMR_IrqHandler()
{
    uint32_t int_state = atomic_begin();

    LPTMR0_CSR |= LPTMR_CSR_TCF_MASK;
    alarm_get_now();

    if (s_is_running && s_alarm_dt == 0)
    {
        s_is_running = false;
        alarm_fired();
    }
    else
    {
        set_alarm();
    }

    atomic_end(int_state);
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
