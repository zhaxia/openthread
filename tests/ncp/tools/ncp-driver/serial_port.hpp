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

#ifndef SERIAL_PORT_HPP_
#define SERIAL_PORT_HPP_

#include <openthread-types.h>
#include <common/code_utils.hpp>

namespace Thread {

ThreadError serial_enable();
ThreadError serial_disable();

ThreadError serial_send(const uint8_t *buf, uint16_t buf_length);

int serial_get_fd();
ThreadError serial_read(uint8_t *buf, uint16_t &buf_length);

}  // namespace Thread

#endif  // SERIAL_PORT_HPP_
