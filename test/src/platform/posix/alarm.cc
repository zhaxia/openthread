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
#include <common/code_utils.h>
#include <common/timer.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void alarm_fired();

static void *alarm_thread(void *arg);

static bool s_is_running = false;
static uint32_t s_alarm = 0;
static timeval s_start;

static pthread_t s_thread;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_cond = PTHREAD_COND_INITIALIZER;

void alarm_init()
{
    gettimeofday(&s_start, NULL);
    pthread_create(&s_thread, NULL, alarm_thread, NULL);
}

uint32_t alarm_get_now()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    timersub(&tv, &s_start, &tv);

    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void alarm_start_at(uint32_t t0, uint32_t dt)
{
    pthread_mutex_lock(&s_mutex);
    s_alarm = t0 + dt;
    s_is_running = true;
    pthread_mutex_unlock(&s_mutex);
    pthread_cond_signal(&s_cond);
}

void alarm_stop()
{
    pthread_mutex_lock(&s_mutex);
    s_is_running = false;
    pthread_mutex_unlock(&s_mutex);
}

void *alarm_thread(void *arg)
{
    int32_t remaining;
    struct timeval tva;
    struct timeval tvb;
    struct timespec ts;

    while (1)
    {
        pthread_mutex_lock(&s_mutex);

        if (!s_is_running)
        {
            // alarm is not running, wait indefinitely
            pthread_cond_wait(&s_cond, &s_mutex);
            pthread_mutex_unlock(&s_mutex);
        }
        else
        {
            // alarm is running
            remaining = s_alarm - alarm_get_now();

            if (remaining > 0)
            {
                // alarm has not passed, wait
                gettimeofday(&tva, NULL);
                tvb.tv_sec = remaining / 1000;
                tvb.tv_usec = (remaining % 1000) * 1000;
                timeradd(&tva, &tvb, &tva);

                ts.tv_sec = tva.tv_sec;
                ts.tv_nsec = tva.tv_usec * 1000;

                pthread_cond_timedwait(&s_cond, &s_mutex, &ts);
                pthread_mutex_unlock(&s_mutex);
            }
            else
            {
                // alarm has passed, signal
                s_is_running = false;
                pthread_mutex_unlock(&s_mutex);
                alarm_fired();
            }
        }
    }
    return NULL;
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
