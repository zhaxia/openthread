/*
 *    Copyright 2016 Nest Labs Inc. All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
