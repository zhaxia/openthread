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
 *   This file includes definitions for managing Thread Network Data.
 */

#ifndef NETWORK_DATA_HPP_
#define NETWORK_DATA_HPP_

#include <common/thread_error.hpp>
#include <thread/lowpan.hpp>
#include <thread/network_data_tlvs.hpp>

namespace Thread {

/**
 * @addtogroup core-netdata
 * @brief
 *   This module includes definitions for generating, processing, and managing Thread Network Data.
 *
 * @{
 *
 * @defgroup core-netdata-core Core
 * @defgroup core-netdata-leader Leader
 * @defgroup core-netdata-local Local
 * @defgroup core-netdata-tlvs TLVs
 *
 * @}
 *
 */

/**
 * @namespace Thread::NetworkData
 *
 * @brief
 *   This namespace includes definitions for managing Thread Network Data.
 *
 */
namespace NetworkData {

/**
 * @addtogroup core-netdata-core
 *
 * @brief
 *   This module includes definitions for managing Thread Network Data.
 *
 * @{
 *
 */

/**
 * This class implements Network Data processing.
 *
 */
class NetworkData
{
public:
    enum
    {
        kMaxSize = 256,  ///< Maximum size of Thread Network Data in bytes.
    };

    /**
     * This method provides a full or stable copy of the Thread Network Data.
     *
     * @param[in]     aStable      TRUE when copying the stable version, FALSE when copying the full version.
     * @param[out]    aData        A pointer to the data buffer.
     * @param[inout]  aDataLength  On entry, size of the data buffer pointed to by @p aData.
     *                             On exit, number of copied bytes.
     */
    void GetNetworkData(bool aStable, uint8_t *aData, uint8_t &aDataLength);

protected:
    /**
     * This method initializes the Thread Network Data.
     *
     */
    void Init() { mLength = 0; }

    /**
     * This method returns a pointer to the Border Router TLV within a given Prefix TLV.
     *
     * @param[in]  aPrefix  A reference to the Prefix TLV.
     *
     * @returns A pointer to the Border Router TLV if one is found or NULL if no Border Router TLV exists.
     *
     */
    BorderRouterTlv *FindBorderRouter(PrefixTlv &aPrefix);

    /**
     * This method returns a pointer to the stable or non-stable Border Router TLV within a given Prefix TLV.
     *
     * @param[in]  aPrefix  A reference to the Prefix TLV.
     * @param[in]  aStable  TRUE if requesting a stable Border Router TLV, FALSE otherwise.
     *
     * @returns A pointer to the Border Router TLV if one is found or NULL if no Border Router TLV exists.
     *
     */
    BorderRouterTlv *FindBorderRouter(PrefixTlv &aPrefix, bool aStable);

    /**
     * This method returns a pointer to the Has Route TLV within a given Prefix TLV.
     *
     * @param[in]  aPrefix  A reference to the Prefix TLV.
     *
     * @returns A pointer to the Has Route TLV if one is found or NULL if no Has Route TLV exists.
     *
     */
    HasRouteTlv *FindHasRoute(PrefixTlv &aPrefix);

    /**
     * This method returns a pointer to the stable or non-stable Has Route TLV within a given Prefix TLV.
     *
     * @param[in]  aPrefix  A reference to the Prefix TLV.
     * @param[in]  aStable  TRUE if requesting a stable Has Route TLV, FALSE otherwise.
     *
     * @returns A pointer to the Has Route TLV if one is found or NULL if no Has Route TLV exists.
     *
     */
    HasRouteTlv *FindHasRoute(PrefixTlv &aPrefix, bool aStable);

    /**
     * This method returns a pointer to the Context TLV within a given Prefix TLV.
     *
     * @param[in]  aPrefix  A reference to the Prefix TLV.
     *
     * @returns A pointer to the Context TLV is one is found or NULL if no Context TLV exists.
     *
     */
    ContextTlv *FindContext(PrefixTlv &aPrefix);

    /**
     * This method returns a pointer to a Prefix TLV.
     *
     * @param[in]  aPrefix        A pointer to an IPv6 prefix.
     * @param[in]  aPrefixLength  The prefix length pointed to by @p aPrefix.
     *
     * @returns A pointer to the Prefix TLV is one is found or NULL if no matching Prefix TLV exists.
     *
     */
    PrefixTlv *FindPrefix(const uint8_t *aPrefix, uint8_t aPrefixLength);

    /**
     * This method inserts bytes into the Network Data.
     *
     * @param[in]  aStart   A pointer to the beginning of the insertion.
     * @param[in]  aLength  The number of bytes to insert.
     *
     * @retval kThreadError_None         Successfully inserted bytes.
     * @retval kThreadError_NoBufs       Insufficient buffer space to insert bytes.
     *
     */
    ThreadError Insert(uint8_t *aStart, uint8_t aLength);

    /**
     * This method removes bytes from the Network Data.
     *
     * @param[in]  aStart   A pointer to the beginning of the removal.
     * @param[in]  aLength  The number of bytes to remove.
     *
     * @retval kThreadError_None    Successfully removed bytes.
     *
     */
    ThreadError Remove(uint8_t *aStart, uint8_t aLength);

    /**
     * This method strips non-stable data from the Thread Network Data.
     *
     * @param[inout]  aData        A pointer to the Network Data to modify.
     * @param[inout]  aDataLength  On entry, the size of the Network Data in bytes.  On exit, the size of the
     *                             resulting Network Data in bytes.
     *
     */
    void RemoveTemporaryData(uint8_t *aData, uint8_t &aDataLength);

    /**
     * This method strips non-stable Sub-TLVs from a Prefix TLV.
     *
     * @param[inout]  aData        A pointer to the Network Data to modify.
     * @param[inout]  aDataLength  On entry, the size of the Network Data in bytes.  On exit, the size of the
     *                             resulting Network Data in bytes.
     * @param[inout]  aPrefix      A reference to the Prefix TLV to modify.
     *
     */
    void RemoveTemporaryData(uint8_t *aData, uint8_t &aDataLength, PrefixTlv &aPrefix);

    /**
     * This method computes the number of IPv6 Prefix bits that match.
     *
     * @param[in]  a        A pointer to the first IPv6 Prefix.
     * @param[in]  b        A pointer to the second IPv6 prefix.
     * @param[in]  aLength  The aximum length in bits to compare.
     *
     * @returns The number of matching bits.
     *
     */
    int8_t PrefixMatch(const uint8_t *a, const uint8_t *b, uint8_t aLength);

    uint8_t mTlvs[kMaxSize];  ///< The Network Data buffer.
    uint8_t mLength;          ///< The number of valid bytes in @var mTlvs.
};

}  // namespace NetworkData

/**
 * @}
 */

}  // namespace Thread

#endif  // NETWORK_DATA_HPP_
