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
 *   This file includes definitions for maintaining Thread network topologies.
 */

#ifndef TOPOLOGY_HPP_
#define TOPOLOGY_HPP_

#include <mac/mac_frame.hpp>
#include <net/ip6.hpp>

namespace Thread {

/**
 * This class represents a Thread neighbor.
 *
 */
class Neighbor
{
public:
    Mac::ExtAddress mMacAddr;            ///< The IEEE 802.15.4 Extended Address
    uint32_t        mLastHeard;          ///< Time when last heard.
    union
    {
        struct
        {
            uint32_t mLinkFrameCounter;  ///< The Link Frame Counter
            uint32_t mMleFrameCounter;   ///< The MLE Frame Counter
            uint16_t mRloc16;            ///< The RLOC16
        } mValid;
        struct
        {
            uint8_t mChallenge[8];       ///< The challenge value
            uint8_t mChallengeLength;    ///< The challenge length
        } mPending;
    };

    /**
     * Neighbor link states.
     *
     */
    enum State
    {
        kStateInvalid,                   ///< Neighbor link is invaild
        kStateParentRequest,             ///< Received an MLE Parent Request message
        kStateChildIdRequest,            ///< Received an MLE Child ID Request message
        kStateLinkRequest,               ///< Sent a MLE Link Request message
        kStateValid,                     ///< Link is valid
    };
    State   mState : 3;                  ///< The link state
    uint8_t mMode : 4;                   ///< The MLE device mode
    bool    mPreviousKey : 1;            ///< Indicates whether or not the neighbor is still using a previous key
    bool    mDataRequest : 1;            ///< Indicates whether or not a Data Poll was received
    int8_t  mRssi;                       ///< Received Signal Strengh Indicator
};

/**
 * This class represents a Thread Child.
 *
 */
class Child : public Neighbor
{
public:
    enum
    {
        kMaxIp6AddressPerChild = 4,
    };
    Ip6::Address mIp6Address[kMaxIp6AddressPerChild];  ///< Registered IPv6 addresses
    uint32_t     mTimeout;                             ///< Child timeout
    uint16_t     mFragmentOffset;                      ///< 6LoWPAN fragment offset
    uint8_t      mRequestTlvs[4];                      ///< Requested MLE TLVs
    uint8_t      mNetworkDataVersion;                  ///< Current Network Data version
};

/**
 * This class represents a Thread Router
 *
 */
class Router : public Neighbor
{
public:
    uint8_t mNextHop;             ///< The next hop towards this router
    uint8_t mLinkQualityOut : 2;  ///< The link quality out for this router
    uint8_t mLinkQualityIn : 2;   ///< The link quality in for this router
    uint8_t mCost : 4;            ///< The cost to this router
    bool    mAllocated : 1;       ///< Indicates whether or not this entry is allocated
    bool    mReclaimDelay : 1;    ///< Indicates whether or not this entry is waiting to be reclaimed
};

}  // namespace Thread

#endif  // TOPOLOGY_HPP_
