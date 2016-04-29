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
 *   This file includes definitions for IEEE 802.15.4 frame filtering based on MAC address.
 */

#ifndef MAC_WHITELIST_HPP_
#define MAC_WHITELIST_HPP_

#include <stdint.h>

#include <common/thread_error.hpp>
#include <mac/mac_frame.hpp>

namespace Thread {
namespace Mac {

/**
 * @addtogroup core-mac
 *
 * @{
 *
 */

/**
 * This class implements whitelist filtering on IEEE 802.15.4 frames.
 *
 */
class Whitelist
{
public:
    enum
    {
        kMaxEntries = 32,
    };

    /**
     * This structure represents a whitelist entry.
     *
     */
    struct Entry
    {
        ExtAddress mExtAddress;        ///< The IEEE 802.15.4 Extended Address.
        int8_t     mRssi;              ///< The constant RSSI value.
        bool       mValid : 1;         ///< TRUE if the entry is valid, FALSE otherwise.
        bool       mConstantRssi : 1;  ///< TRUE if the constant RSSI value is used, FALSE otherwise.
    };

    /**
     * This method initializes the whitelist filter.
     *
     */
    void Init(void);

    /**
     * This method enables the whitelist filter.
     *
     */
    void Enable(void);

    /**
     * This method disables the whitelist filter.
     *
     */
    void Disable(void);

    /**
     * This method indicates whether or not the whitelist filter is enabled.
     *
     * @retval TRUE   If the whitelist filter is enabled.
     * @retval FALSE  If the whitelist filter is disabled.
     *
     */
    bool IsEnabled(void) const;

    /**
     * This method returns the maximum number of whitelist entries.
     *
     * @returns The maximum number of whitelist entries.
     *
     */
    int GetMaxEntries(void) const;

    /**
     * This method returns the whitelist entries.
     *
     * @returns The whitelist entries.
     *
     */
    const Entry *GetEntries(void) const;

    /**
     * This method adds an Extended Address to the whitelist filter.
     *
     * @param[in]  aAddress  A reference to the Extended Address.
     *
     * @returns A pointer to the whitelist entry or NULL if there are no available entries.
     *
     */
    Entry *Add(const ExtAddress &aAddress);

    /**
     * This method removes an Extended Address to the whitelist filter.
     *
     * @param[in]  aAddress  A reference to the Extended Address.
     *
     */
    void Remove(const ExtAddress &aAddress);

    /**
     * This method removes all entries from the whitelist filter.
     *
     */
    void Clear(void);

    /**
     * This method finds a whitelist entry.
     *
     * @param[in]  aAddress  A reference to the Extended Address.
     *
     * @returns A pointer to the whitelist entry or NULL if the entry could not be found.
     *
     */
    Entry *Find(const ExtAddress &aAddress);

    /**
     * This method clears the constant RSSI value and uses the measured value provided by the radio instead.
     *
     * @param[in]  aEntry  A reference to the whitelist entry.
     *
     */
    void ClearConstantRssi(Entry &aEntry);

    /**
     * This method indicates whether or not the constant RSSI is set.
     *
     * @param[in]   aEntry  A reference to the whitelist entry.
     * @param[out]  aRssi   A reference to the RSSI variable.
     *
     * @retval kThreadError_None        A constant RSSI is set and written to @p aRssi.
     * @retval kThreadError_InvalidArg  A constnat RSSI was not set.
     *
     */
    ThreadError GetConstantRssi(Entry &aEntry, int8_t &aRssi) const;

    /**
     * This method sets a constant RSSI value for all received messages matching @p aEntry.
     *
     * @param[in]  aEntry  A reference to the whitelist entry.
     * @param[in]  aRssi   An RSSI value in dBm.
     *
     */
    void SetConstantRssi(Entry &aEntry, int8_t aRssi);

private:
    Entry mWhitelist[kMaxEntries];

    bool mEnabled = false;
};

/**
 * @}
 *
 */

}  // namespace Mac
}  // namespace Thread

#endif  // MAC_WHITELIST_HPP_
