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
 */

/**
 *    @file
 *    @brief
 *      Defines the interface to the Alarm object that drives Thread
 *      timers.
 *
 *    @author    Jonathan Hui <jonhui@nestlabs.com>
 */

#ifndef ALARM_H_
#define ALARM_H_

#include <stdint.h>

namespace Thread {

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Alarm resources.  This will typically initilize
 * hardware resources that are used to implement the alarm.
 */
void alarm_init();

/**
 * Set the alarm to fire at a time delay relative from t0.
 *
 * @param[in] t0  The reference time.
 * @param[in] dt  The time delay in milliseconds.
 */
void alarm_start_at(uint32_t t0, uint32_t dt);

/**
 * Stop the alarm.
 */
void alarm_stop();

/**
 * Get the current current time.
 *
 * @return The current time.
 */
uint32_t alarm_get_now();

#ifdef __cplusplus
}  // end of extern "C"
#endif

}  // namespace Thread

#endif  // ALARM_H_
