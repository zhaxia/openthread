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
 * @brief
 *   This file includes the platform abstraction for true random number generation.
 */

#ifndef AES_ECB_H_
#define AES_ECB_H_

#include <stdint.h>

#include <openthread-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup core-security
 *
 * @{
 *
 */

enum
{
    otAesBlockSize = 16,  ///< AES-128 block size.
};

/**
 * This method sets the key.
 *
 * @param[in]  aKey        A pointer to the key.
 * @param[in]  aKeyLength  Length of the key in bytes.
 *
 */
void otCryptoAesEcbSetKey(const void *aKey, uint16_t aKeyLength);

/**
 * This method encrypts data.
 *
 * @param[in]   aInput   A pointer to the input.
 * @param[out]  aOutput  A pointer to the output.
 *
 */
void otCryptoAesEcbEncrypt(const uint8_t aInput[otAesBlockSize], uint8_t aOutput[otAesBlockSize]);

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // AES_ECB_H_
