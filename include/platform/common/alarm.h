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
 *      Maps to the appropriate platform-specific alarm.h.
 *
 *    @author    Jonathan Hui <jonhui@nestlabs.com>
 */

#ifndef PLATFORM_COMMON_ALARM_H_
#define PLATFORM_COMMON_ALARM_H_

#ifdef CPU_KW2X
#include "platform/kw2x/alarm.h"
#elif CPU_KW4X
#include "platform/kw4x/alarm.h"
#elif CPU_DA15100
#include "platform/da15100/alarm.h"
#elif CPU_EM35X
#include "platform/em35x/alarm.h"
#else
#include "platform/posix/alarm.h"
#endif

#endif  // PLATFORM_COMMON_ALARM_H_
