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
 *   This file contains definitions for the error codes used throughout the Thread library.
 */

#ifndef THREAD_ERROR_HPP_
#define THREAD_ERROR_HPP_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This enumeration represents error codes used throughout OpenThread.
 */
typedef enum ThreadError
{
    kThreadError_None = 0,
    kThreadError_Failed = 1,
    kThreadError_Drop = 2,
    kThreadError_NoBufs = 3,
    kThreadError_NoRoute = 4,
    kThreadError_Busy = 5,
    kThreadError_Parse = 6,
    kThreadError_InvalidArgs = 7,
    kThreadError_Security = 8,
    kThreadError_LeaseQuery = 9,
    kThreadError_NoAddress = 10,
    kThreadError_NotReceiving = 11,
    kThreadError_Abort = 12,
    kThreadError_NotImplemented = 13,
    kThreadError_InvalidState = 14,
    kThreadError_Error = 255,
} ThreadError;

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // THREAD_ERROR_HPP_
