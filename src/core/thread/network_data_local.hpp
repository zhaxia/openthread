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
 *   This file includes definitions for manipulating local Thread Network Data.
 */

#ifndef NETWORK_DATA_LOCAL_HPP_
#define NETWORK_DATA_LOCAL_HPP_

#include <net/udp6.hpp>
#include <thread/mle_router.hpp>
#include <thread/network_data.hpp>

namespace Thread {

class ThreadNetif;

/**
 * @addtogroup core-netdata-local
 *
 * @brief
 *   This module includes definitions for manipulating local Thread Network Data.
 *
 * @{
 */

namespace NetworkData {

/**
 * This class implements the Thread Network Data contributed by the local device.
 *
 */
class Local: public NetworkData
{
public:
    /**
     * This constructor initializes the local Network Data.
     *
     * @param[in]  aNetif  A reference to the Thread network interface.
     *
     */
    explicit Local(ThreadNetif &aNetif);

    /**
     * This method adds a Border Router entry to the Thread Network Data.
     *
     * @param[in]  aPrefix        A pointer to the prefix.
     * @param[in]  aPrefixLength  The length of @p aPrefix in bytes.
     * @param[in]  aPrf           The preference value.
     * @param[in]  aFlags         The Border Router Flags value.
     * @param[in]  aStable        The Stable value.
     *
     * @retval kThreadError_None    Successfully added the Border Router entry.
     * @retval kThreadError_NoBufs  Insufficient space to add the Border Router entry.
     *
     */
    ThreadError AddOnMeshPrefix(const uint8_t *aPrefix, uint8_t aPrefixLength, int8_t aPrf, uint8_t aFlags,
                                bool aStable);

    /**
     * This method removes a Border Router entry from the Thread Network Data.
     *
     * @param[in]  aPrefix        A pointer to the prefix.
     * @param[in]  aPrefixLength  The length of @p aPrefix in bytes.
     *
     * @retval kThreadError_None      Successfully removed the Border Router entry.
     * @retval kThreadError_NotFound  Could not find the Border Router entry.
     *
     */
    ThreadError RemoveOnMeshPrefix(const uint8_t *aPrefix, uint8_t aPrefixLength);

    /**
     * This method adds a Has Route entry to the Thread Network data.
     *
     * @param[in]  aPrefix        A pointer to the prefix.
     * @param[in]  aPrefixLength  The length of @p aPrefix in bytes.
     * @param[in]  aPrf           The preference value.
     * @param[in]  aStable        The Stable value.
     *
     * @retval kThreadError_None    Successfully added the Has Route entry.
     * @retval kThreadError_NoBufs  Insufficient space to add the Has Route entry.
     *
     */
    ThreadError AddHasRoutePrefix(const uint8_t *aPrefix, uint8_t aPrefixLength, int8_t aPrf, bool aStable);

    /**
     * This method removes a Border Router entry from the Thread Network Data.
     *
     * @param[in]  aPrefix        A pointer to the prefix.
     * @param[in]  aPrefixLength  The length of @p aPrefix in bytes.
     *
     * @retval kThreadError_None      Successfully removed the Border Router entry.
     * @retval kThreadError_NotFound  Could not find the Border Router entry.
     *
     */
    ThreadError RemoveHasRoutePrefix(const uint8_t *aPrefix, uint8_t aPrefixLength);

    /**
     * This method sends a Server Data Registration message to the Leader.
     *
     * @param[in]  aDestination  The IPv6 destination.
     *
     * @retval kThreadError_None    Successfully enqueued the registration message.
     * @retval kThreadError_NoBufs  Insufficient message buffers to generate the registration message.
     *
     */
    ThreadError Register(const Ip6::Address &aDestination);

private:
    static void HandleUdpReceive(void *aContext, otMessage aMessage, const otMessageInfo *aMessageInfo);
    void HandleUdpReceive(Message &aMessage, const Ip6::MessageInfo &aMessageInfo);

    ThreadError UpdateRloc(void);
    ThreadError UpdateRloc(PrefixTlv &aPrefix);
    ThreadError UpdateRloc(HasRouteTlv &aHasRoute);
    ThreadError UpdateRloc(BorderRouterTlv &aBorderRouter);

    Ip6::UdpSocket  mSocket;
    uint8_t         mCoapToken[2];
    uint16_t        mCoapMessageId;

    Mle::MleRouter &mMle;
};

}  // namespace NetworkData

/**
 * @}
 */

}  // namespace Thread

#endif  // NETWORK_DATA_LOCAL_HPP_
