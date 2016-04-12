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

#ifndef TOPOLOGY_H_
#define TOPOLOGY_H_

#include <mac/mac_frame.h>
#include <net/ip6.h>

namespace Thread {

class Neighbor
{
public:
    Mac::Address64 mac_addr;
    uint32_t last_heard;
    union
    {
        struct
        {
            uint32_t link_frame_counter;
            uint32_t mle_frame_counter;
            uint16_t rloc16;
        } valid;
        struct
        {
            uint8_t challenge[8];
            uint8_t challenge_length;
        } pending;
    };

    enum State
    {
        kStateInvalid = 0,
        kStateParentRequest = 1,
        kStateChildIdRequest = 2,
        kStateLinkRequest = 3,
        kStateValid = 4,
    };
    State state : 3;
    uint8_t mode : 4;
    bool previous_key : 1;
    bool frame_pending : 1;
    bool data_request : 1;
    bool allocated : 1;
    bool reclaim_delay : 1;
    int8_t rssi;
};

class Child : public Neighbor
{
public:
    enum
    {
        kMaxIp6AddressPerChild = 4,
    };
    Ip6Address ip6_address[kMaxIp6AddressPerChild];
    uint32_t timeout;
    uint16_t fragment_offset;
    uint8_t request_tlvs[4];
    uint8_t network_data_version;
};

class Router : public Neighbor
{
public:
    uint8_t nexthop;
    uint8_t link_quality_out : 2;
    uint8_t link_quality_in : 2;
    uint8_t cost : 4;
};

}  // namespace Thread

#endif  // TOPOLOGY_H_
