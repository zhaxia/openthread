/*
 *
 *    Copyright (c) 2015 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    @author  Jonathan Hui <jonhui@nestlabs.com>
 *
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
