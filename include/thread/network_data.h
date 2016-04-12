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

#ifndef NETWORK_DATA_H_
#define NETWORK_DATA_H_

#include <common/thread_error.h>
#include <thread/lowpan.h>
#include <thread/network_data_tlvs.h>

namespace Thread {
namespace NetworkData {

class NetworkData
{
public:
    ThreadError GetNetworkData(bool stable, uint8_t *data, uint8_t &data_length);

protected:
    BorderRouterTlv *FindBorderRouter(PrefixTlv &prefix);
    BorderRouterTlv *FindBorderRouter(PrefixTlv &prefix, bool stable);
    HasRouteTlv *FindHasRoute(PrefixTlv &prefix);
    HasRouteTlv *FindHasRoute(PrefixTlv &prefix, bool stable);
    PrefixTlv *FindPrefix(const uint8_t *prefix, uint8_t prefix_length);

    ThreadError Insert(uint8_t *start, uint8_t length);
    ThreadError Remove(uint8_t *start, uint8_t length);
    ThreadError RemoveTemporaryData(uint8_t *data, uint8_t &data_length);
    ThreadError RemoveTemporaryData(uint8_t *data, uint8_t &data_length, PrefixTlv &prefix);
    int8_t PrefixMatch(const uint8_t *a, const uint8_t *b, uint8_t length);

    uint8_t m_tlvs[256];
    uint8_t m_length = 0;
};

}  // namespace NetworkData
}  // namespace Thread

#endif  // NETWORK_DATA_H_
