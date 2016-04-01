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
#include <core/cpu.h>
#include <cpu/CpuTick.hpp>

#ifdef __cplusplus
extern "C" {
#endif

#define CLOCK_TO_MSEC  (CPU_DEFAULT_CLOCK_HZ/1000)

extern void alarm_fired();

static uint32_t s_counter;
static uint32_t s_alarm_t0  = 0;
static uint32_t s_alarm_dt = 0;
static bool s_is_running = false;

void alarm_init()
{
    s_counter = 0;
    s_is_running = false;

    SysTick->LOAD = CLOCK_TO_MSEC - 1;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

uint32_t alarm_get_now()
{
    return s_counter;
}

void alarm_start_at(uint32_t t0, uint32_t dt)
{
    uint32_t int_state = atomic_begin();

    s_alarm_t0 = t0;
    s_alarm_dt = dt;
    s_is_running = true;

    atomic_end(int_state);
}

void alarm_stop()
{
    s_is_running = false;
}

extern "C" void SysTick_Handler()
{
    uint32_t int_state = atomic_begin();
    bool fire = false;

    s_counter++;

    if (s_is_running)
    {
        uint32_t expires = s_alarm_t0 + s_alarm_dt;

        if (s_alarm_t0 <= s_counter)
        {
            if (expires >= s_alarm_t0 && expires <= s_counter)
            {
                fire = true;
            }
        }
        else
        {
            if (expires >= s_alarm_t0 || expires <= s_counter)
            {
                fire = true;
            }
        }

        if (fire)
        {
            s_is_running = false;
            alarm_fired();
        }
    }

    atomic_end(int_state);
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
