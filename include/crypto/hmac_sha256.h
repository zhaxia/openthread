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
 *   This file includes the platform abstraction for HMAC SHA-256 computations.
 */

#ifndef HMAC_SHA256_H_
#define HMAC_SHA256_H_

#include <stdint.h>

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
    otCryptoSha256Size = 32,  ///< SHA-256 hash size (bytes)
};

/**
 * This method sets the key.
 *
 * @param[in]  aKey        A pointer to the key.
 * @param[in]  aKeyLength  The key length in bytes.
 *
 */
void otCryptoHmacSha256Start(const void *aKey, uint16_t aKeyLength);

/**
 * This method inputs bytes into the HMAC computation.
 *
 * @param[in]  aBuf        A pointer to the input buffer.
 * @param[in]  aBufLength  The length of @p aBuf in bytes.
 *
 */
void otCryptoHmacSha256Update(const void *aBuf, uint16_t aBufLength);

/**
 * This method finalizes the hash computation.
 *
 * @param[out]  aHash  A pointer to the output buffer.
 *
 */
void otCryptoHmacSha256Finish(uint8_t aHash[otCryptoSha256Size]);

/**
 * @}
 *
 */

#ifdef __cplusplus
}  // end of extern "C"
#endif

#endif  // HMAC_SHA256_H_
