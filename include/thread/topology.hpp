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

class Neighbor
{
public:
    Mac::ExtAddress mMacAddr;
    uint32_t mLastHeard;
    union
    {
        struct
        {
            uint32_t mLinkFrameCounter;
            uint32_t mMleFrameCounter;
            uint16_t mRloc16;
        } mValid;
        struct
        {
            uint8_t mChallenge[8];
            uint8_t mChallengeLength;
        } mPending;
    };

    enum State
    {
        kStateInvalid = 0,
        kStateParentRequest = 1,
        kStateChildIdRequest = 2,
        kStateLinkRequest = 3,
        kStateValid = 4,
    };
    State mState : 3;
    uint8_t mMode : 4;
    bool mPreviousKey : 1;
    bool mFramePending : 1;
    bool mDataRequest : 1;
    bool mAllocated : 1;
    bool mReclaimDelay : 1;
    int8_t mRssi;
};

class Child : public Neighbor
{
public:
    enum
    {
        kMaxIp6AddressPerChild = 4,
    };
    Ip6::Address mIp6Address[kMaxIp6AddressPerChild];
    uint32_t mTimeout;
    uint16_t mFragmentOffset;
    uint8_t mRequestTlvs[4];
    uint8_t mNetworkDataVersion;
};

class Router : public Neighbor
{
public:
    uint8_t mNextHop;
    uint8_t mLinkQualityOut : 2;
    uint8_t mLinkQualityIn : 2;
    uint8_t mCost : 4;
};

}  // namespace Thread

#endif  // TOPOLOGY_HPP_
