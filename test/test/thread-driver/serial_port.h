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

#ifndef SERIAL_PORT_H_
#define SERIAL_PORT_H_

#include <common/code_utils.h>
#include <common/thread_error.h>

#ifdef __cplusplus
extern "C" {
#endif

ThreadError uart_start();
ThreadError uart_stop();

ThreadError uart_send(const uint8_t *buf, uint16_t buf_length);

int uart_get_fd();
ThreadError uart_read();

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // SERIAL_PORT_H_
