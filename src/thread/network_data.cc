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

#include <thread/network_data.h>
#include <common/code_utils.h>
#include <common/debug.h>

namespace Thread {
namespace NetworkData {

ThreadError NetworkData::GetNetworkData(bool stable, uint8_t *data, uint8_t &data_length)
{
    assert(data != NULL);

    memcpy(data, m_tlvs, m_length);
    data_length = m_length;

    if (stable)
    {
        RemoveTemporaryData(data, data_length);
    }

    return kThreadError_None;
}

ThreadError NetworkData::RemoveTemporaryData(uint8_t *data, uint8_t &data_length)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(data);
    NetworkDataTlv *end;
    PrefixTlv *prefix;
    uint8_t length;
    uint8_t *dst;
    uint8_t *src;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(data + data_length);

        if (cur >= end)
        {
            break;
        }

        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
        {
            prefix = reinterpret_cast<PrefixTlv *>(cur);
            RemoveTemporaryData(data, data_length, *prefix);

            if (prefix->GetSubTlvsLength() == 0)
            {
                length = sizeof(NetworkDataTlv) + cur->GetLength();
                dst = reinterpret_cast<uint8_t *>(cur);
                src = reinterpret_cast<uint8_t *>(cur->GetNext());
                memmove(dst, src, data_length - (dst - data));
                data_length -= length;
                continue;
            }

            dump("remove prefix done", m_tlvs, m_length);
            break;
        }

        default:
        {
            assert(false);
            break;
        }
        }

        cur = cur->GetNext();
    }

    dump("remove done", data, data_length);

    return kThreadError_None;
}

ThreadError NetworkData::RemoveTemporaryData(uint8_t *data, uint8_t &data_length, PrefixTlv &prefix)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end;
    uint8_t length;
    uint8_t *dst;
    uint8_t *src;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

        if (cur >= end)
        {
            break;
        }

        if (cur->IsStable())
        {
            // keep stable tlv
            cur = cur->GetNext();
        }
        else
        {
            // remove temporary tlv
            length = sizeof(NetworkDataTlv) + cur->GetLength();
            dst = reinterpret_cast<uint8_t *>(cur);
            src = reinterpret_cast<uint8_t *>(cur->GetNext());
            memmove(dst, src, data_length - (dst - data));
            prefix.SetSubTlvsLength(prefix.GetSubTlvsLength() - length);
            data_length -= length;
        }
    }

    return kThreadError_None;
}

BorderRouterTlv *NetworkData::FindBorderRouter(PrefixTlv &prefix)
{
    BorderRouterTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeBorderRouter)
        {
            ExitNow(rval = reinterpret_cast<BorderRouterTlv *>(cur));
        }

        cur = cur->GetNext();
    }

exit:
    return rval;
}

BorderRouterTlv *NetworkData::FindBorderRouter(PrefixTlv &prefix, bool stable)
{
    BorderRouterTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeBorderRouter &&
            cur->IsStable() == stable)
        {
            ExitNow(rval = reinterpret_cast<BorderRouterTlv *>(cur));
        }

        cur = cur->GetNext();
    }

exit:
    return rval;
}

HasRouteTlv *NetworkData::FindHasRoute(PrefixTlv &prefix)
{
    HasRouteTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeHasRoute)
        {
            ExitNow(rval = reinterpret_cast<HasRouteTlv *>(cur));
        }

        cur = cur->GetNext();
    }

exit:
    return rval;
}

HasRouteTlv *NetworkData::FindHasRoute(PrefixTlv &prefix, bool stable)
{
    HasRouteTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(prefix.GetSubTlvs() + prefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeHasRoute &&
            cur->IsStable() == stable)
        {
            ExitNow(rval = reinterpret_cast<HasRouteTlv *>(cur));
        }

        cur = cur->GetNext();
    }

exit:
    return rval;
}

PrefixTlv *NetworkData::FindPrefix(const uint8_t *prefix, uint8_t prefix_length)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(m_tlvs);
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(m_tlvs + m_length);
    PrefixTlv *compare;

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypePrefix)
        {
            compare = reinterpret_cast<PrefixTlv *>(cur);

            if (compare->GetPrefixLength() == prefix_length &&
                PrefixMatch(compare->GetPrefix(), prefix, prefix_length) >= prefix_length)
            {
                return compare;
            }
        }

        cur = cur->GetNext();
    }

    return NULL;
}

int8_t NetworkData::PrefixMatch(const uint8_t *a, const uint8_t *b, uint8_t length)
{
    uint8_t rval = 0;
    uint8_t bytes = (length + 7) / 8;
    uint8_t diff;

    for (uint8_t i = 0; i < bytes; i++)
    {
        diff = a[i] ^ b[i];

        if (diff == 0)
        {
            rval += 8;
        }
        else
        {
            while ((diff & 0x80) == 0)
            {
                rval++;
                diff <<= 1;
            }

            break;
        }
    }

    return (rval >= length) ? rval : -1;
}

ThreadError NetworkData::Insert(uint8_t *start, uint8_t length)
{
    assert(length + m_length <= sizeof(m_tlvs) &&
           m_tlvs <= start &&
           start <= m_tlvs + m_length);
    memmove(start + length, start, m_length - (start - m_tlvs));
    m_length += length;
    return kThreadError_None;
}

ThreadError NetworkData::Remove(uint8_t *start, uint8_t length)
{
    assert(length <= m_length &&
           m_tlvs <= start &&
           (start - m_tlvs) + length <= m_length);
    memmove(start, start + length, m_length - ((start - m_tlvs) + length));
    m_length -= length;
    return kThreadError_None;
}

}  // namespace NetworkData
}  // namespace Thread
