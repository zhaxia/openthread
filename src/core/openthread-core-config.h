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

/**
 * @file
 *   This file includes compile-time configuration constants for OpenThread.
 */

#ifndef OPENTHREAD_CORE_CONFIG_H_
#define OPENTHREAD_CORE_CONFIG_H_

/**
 * @def OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS
 *
 * The number of message buffers in the buffer pool.
 *
 */
#ifndef OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS
#define OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS               64
#endif  // OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS

/**
 * @def OPENTHREAD_CONFIG_MESSAGE_BUFFER_SIZE
 *
 * The size of a message buffer in bytes.
 *
 */
#ifndef OPENTHREAD_CONFIG_MESSAGE_BUFFER_SIZE
#define OPENTHREAD_CONFIG_MESSAGE_BUFFER_SIZE               128
#endif  // OPENTHREAD_CONFIG_MESSAGE_BUFFER_SIZE

/**
 * @def OPENTHREAD_CONFIG_DEFAULT_CHANNEL
 *
 * The default IEEE 802.15.4 channel.
 *
 */
#ifndef OPENTHREAD_CONFIG_DEFAULT_CHANNEL
#define OPENTHREAD_CONFIG_DEFAULT_CHANNEL                   11
#endif  // OPENTHREAD_CONFIG_DEFAULT_CHANNEL

/**
 * @def OPENTHREAD_CONFIG_ATTACH_DATA_POLL_PERIOD
 *
 * The Data Poll period during attach in milliseconds.
 *
 */
#ifndef OPENTHREAD_CONFIG_ATTACH_DATA_POLL_PERIOD
#define OPENTHREAD_CONFIG_ATTACH_DATA_POLL_PERIOD           100
#endif  // OPENTHREAD_CONFIG_ATTACH_DATA_POLL_PERIOD

/**
 * @def OPENTHREAD_CONFIG_ADDRESS_CACHE_ENTRIES
 *
 * The number of EID-to-RLOC cache entries.
 *
 */
#ifndef OPENTHREAD_CONFIG_ADDRESS_CACHE_ENTRIES
#define OPENTHREAD_CONFIG_ADDRESS_CACHE_ENTRIES             8
#endif  // OPENTHREAD_CONFIG_ADDRESS_CACHE_ENTRIES

/**
 * @def OPENTHREAD_CONFIG_MAX_CHILDREN
 *
 * The maximum number of children.
 *
 */
#ifndef OPENTHREAD_CONFIG_MAX_CHILDREN
#define OPENTHREAD_CONFIG_MAX_CHILDREN                      5
#endif  // OPENTHREAD_CONFIG_MAX_CHILDREN

/**
 * @def OPENTHREAD_CONFIG_IP_ADDRS_PER_CHILD
 *
 * The minimum number of supported IPv6 address registrations per child.
 *
 */
#ifndef OPENTHREAD_CONFIG_IP_ADDRS_PER_CHILD
#define OPENTHREAD_CONFIG_IP_ADDRS_PER_CHILD                4
#endif  // OPENTHREAD_CONFIG_IP_ADDRS_PER_CHILD

/**
 * @def OPENTHREAD_CONFIG_6LOWPAN_REASSEMBLY_TIMEOUT
 *
 * The 6LoWPAN fragment reassembly timeout in seconds.
 *
 */
#ifndef OPENTHREAD_CONFIG_6LOWPAN_REASSEMBLY_TIMEOUT
#define OPENTHREAD_CONFIG_6LOWPAN_REASSEMBLY_TIMEOUT        5
#endif  // OPENTHREAD_CONFIG_6LOWPAN_REASSEMBLY_TIMEOUT

/**
 * @def OPENTHREAD_CONFIG_MPL_CACHE_ENTRIES
 *
 * The number of MPL cache entries for duplicate detection.
 *
 */
#ifndef OPENTHREAD_CONFIG_MPL_CACHE_ENTRIES
#define OPENTHREAD_CONFIG_MPL_CACHE_ENTRIES                 32
#endif  // OPENTHREAD_CONFIG_MPL_CACHE_ENTRIES

/**
 * @def OPENTHREAD_CONFIG_MPL_CACHE_ENTRY_LIFETIME
 *
 * The MPL cache entry lifetime in seconds.
 *
 */
#ifndef OPENTHREAD_CONFIG_MPL_CACHE_ENTRY_LIFETIME
#define OPENTHREAD_CONFIG_MPL_CACHE_ENTRY_LIFETIME          5
#endif  // OPENTHREAD_CONFIG_MPL_CACHE_ENTRY_LIFETIME

#endif  // OPENTHREAD_CORE_CONFIG_H_

