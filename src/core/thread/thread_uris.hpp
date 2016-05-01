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
 *   This file includes definitions for Thread URIs.
 */

#ifndef THREAD_URIS_HPP_
#define THREAD_URIS_HPP_

namespace Thread {

/**
 * The URI Path for Address Query.
 *
 */
#define OPENTHREAD_URI_ADDRESS_QUERY    "a/aq"

/**
 * @def OPENTHREAD_URI_ADDRESS_NOTIFY
 *
 * The URI Path for Address Notify.
 *
 */
#define OPENTHREAD_URI_ADDRESS_NOTIFY   "a/an"

/**
 * @def OPENTHREAD_URI_ADDRESS_ERROR
 *
 * The URI Path for Address Error.
 *
 */
#define OPENTHREAD_URI_ADDRESS_ERROR    "a/ae"

/**
 * @def OPENTHREAD_URI_ADDRESS_RELEASE
 *
 * The URI Path for Address Release.
 *
 */
#define OPENTHREAD_URI_ADDRESS_RELEASE  "a/ar"

/**
 * @def OPENTHREAD_URI_ADDRESS_SOLICIT
 *
 * The URI Path for Address Solicit.
 *
 */
#define OPENTHREAD_URI_ADDRESS_SOLICIT  "a/as"

/**
 * @def OPENTHREAD_URI_SERVER_DATA
 *
 * The URI Path for Server Data Registration.
 *
 */
#define OPENTHREAD_URI_SERVER_DATA      "n/sd"

}  // namespace Thread

#endif  // THREAD_URIS_HPP_
