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
 *   This file implements common methods for manipulating Thread Network Data.
 */

#include <common/code_utils.hpp>
#include <common/debug.hpp>
#include <thread/network_data.hpp>

namespace Thread {
namespace NetworkData {

void NetworkData::GetNetworkData(bool aStable, uint8_t *aData, uint8_t &aDataLength)
{
    assert(aData != NULL);

    memcpy(aData, mTlvs, mLength);
    aDataLength = mLength;

    if (aStable)
    {
        RemoveTemporaryData(aData, aDataLength);
    }
}

void NetworkData::RemoveTemporaryData(uint8_t *aData, uint8_t &aDataLength)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(aData);
    NetworkDataTlv *end;
    PrefixTlv *prefix;
    uint8_t length;
    uint8_t *dst;
    uint8_t *src;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(aData + aDataLength);

        if (cur >= end)
        {
            break;
        }

        switch (cur->GetType())
        {
        case NetworkDataTlv::kTypePrefix:
        {
            prefix = reinterpret_cast<PrefixTlv *>(cur);
            RemoveTemporaryData(aData, aDataLength, *prefix);

            if (prefix->GetSubTlvsLength() == 0)
            {
                length = sizeof(NetworkDataTlv) + cur->GetLength();
                dst = reinterpret_cast<uint8_t *>(cur);
                src = reinterpret_cast<uint8_t *>(cur->GetNext());
                memmove(dst, src, aDataLength - (src - aData));
                aDataLength -= length;
                continue;
            }

            dump("remove prefix done", mTlvs, mLength);
            break;
        }

        default:
        {
            // remove temporary tlv
            if (!cur->IsStable())
            {
                length = sizeof(NetworkDataTlv) + cur->GetLength();
                dst = reinterpret_cast<uint8_t *>(cur);
                src = reinterpret_cast<uint8_t *>(cur->GetNext());
                memmove(dst, src, aDataLength - (src - aData));
                aDataLength -= length;
                continue;
            }

            break;
        }
        }

        cur = cur->GetNext();
    }

    dump("remove done", aData, aDataLength);
}

void NetworkData::RemoveTemporaryData(uint8_t *aData, uint8_t &aDataLength, PrefixTlv &aPrefix)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs());
    NetworkDataTlv *end;
    BorderRouterTlv *borderRouter;
    HasRouteTlv *hasRoute;
    ContextTlv *context;
    BorderRouterEntry *borderRouterEntry;
    HasRouteEntry *hasRouteEntry;
    uint8_t length;
    uint8_t contextId;
    uint8_t *dst;
    uint8_t *src;

    while (1)
    {
        end = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs() + aPrefix.GetSubTlvsLength());

        if (cur >= end)
        {
            break;
        }

        if (cur->IsStable())
        {
            switch (cur->GetType())
            {
            case NetworkDataTlv::kTypeBorderRouter:
            {
                borderRouter = FindBorderRouter(aPrefix);
                context = FindContext(aPrefix);
                contextId = context->GetContextId();

                // replace p_border_router_16
                for (int i = 0; i < borderRouter->GetNumEntries(); i++)
                {
                    borderRouterEntry = borderRouter->GetEntry(i);

                    if (borderRouterEntry->IsDhcp() || borderRouterEntry->IsConfigure())
                    {
                        borderRouterEntry->SetRloc(0xfc00 | contextId);
                    }
                    else
                    {
                        borderRouterEntry->SetRloc(0xfffe);
                    }
                }

                break;
            }

            case NetworkDataTlv::kTypeHasRoute:
            {
                hasRoute = FindHasRoute(aPrefix);

                // replace r_border_router_16
                for (int j = 0; j < hasRoute->GetNumEntries(); j++)
                {
                    hasRouteEntry = hasRoute->GetEntry(j);
                    hasRouteEntry->SetRloc(0xfffe);
                }

                break;
            }

            default:
            {
                break;
            }
            }

            // keep stable tlv
            cur = cur->GetNext();
        }
        else
        {
            // remove temporary tlv
            length = sizeof(NetworkDataTlv) + cur->GetLength();
            dst = reinterpret_cast<uint8_t *>(cur);
            src = reinterpret_cast<uint8_t *>(cur->GetNext());
            memmove(dst, src, aDataLength - (src - aData));
            aPrefix.SetSubTlvsLength(aPrefix.GetSubTlvsLength() - length);
            aDataLength -= length;
        }
    }
}

BorderRouterTlv *NetworkData::FindBorderRouter(PrefixTlv &aPrefix)
{
    BorderRouterTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs() + aPrefix.GetSubTlvsLength());

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

BorderRouterTlv *NetworkData::FindBorderRouter(PrefixTlv &aPrefix, bool aStable)
{
    BorderRouterTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs() + aPrefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeBorderRouter &&
            cur->IsStable() == aStable)
        {
            ExitNow(rval = reinterpret_cast<BorderRouterTlv *>(cur));
        }

        cur = cur->GetNext();
    }

exit:
    return rval;
}

HasRouteTlv *NetworkData::FindHasRoute(PrefixTlv &aPrefix)
{
    HasRouteTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs() + aPrefix.GetSubTlvsLength());

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

HasRouteTlv *NetworkData::FindHasRoute(PrefixTlv &aPrefix, bool aStable)
{
    HasRouteTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs() + aPrefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeHasRoute &&
            cur->IsStable() == aStable)
        {
            ExitNow(rval = reinterpret_cast<HasRouteTlv *>(cur));
        }

        cur = cur->GetNext();
    }

exit:
    return rval;
}

ContextTlv *NetworkData::FindContext(PrefixTlv &aPrefix)
{
    ContextTlv *rval = NULL;
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs());
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(aPrefix.GetSubTlvs() + aPrefix.GetSubTlvsLength());

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypeContext)
        {
            ExitNow(rval = reinterpret_cast<ContextTlv *>(cur));
        }

        cur = cur->GetNext();
    }

exit:
    return rval;
}

PrefixTlv *NetworkData::FindPrefix(const uint8_t *aPrefix, uint8_t aPrefixLength)
{
    NetworkDataTlv *cur = reinterpret_cast<NetworkDataTlv *>(mTlvs);
    NetworkDataTlv *end = reinterpret_cast<NetworkDataTlv *>(mTlvs + mLength);
    PrefixTlv *compare;

    while (cur < end)
    {
        if (cur->GetType() == NetworkDataTlv::kTypePrefix)
        {
            compare = reinterpret_cast<PrefixTlv *>(cur);

            if (compare->GetPrefixLength() == aPrefixLength &&
                PrefixMatch(compare->GetPrefix(), aPrefix, aPrefixLength) >= aPrefixLength)
            {
                return compare;
            }
        }

        cur = cur->GetNext();
    }

    return NULL;
}

int8_t NetworkData::PrefixMatch(const uint8_t *a, const uint8_t *b, uint8_t aLength)
{
    uint8_t rval = 0;
    uint8_t bytes = BitVectorBytes(aLength);
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

    return (rval >= aLength) ? rval : -1;
}

ThreadError NetworkData::Insert(uint8_t *aStart, uint8_t aLength)
{
    assert(aLength + mLength <= sizeof(mTlvs) &&
           mTlvs <= aStart &&
           aStart <= mTlvs + mLength);
    memmove(aStart + aLength, aStart, mLength - (aStart - mTlvs));
    mLength += aLength;
    return kThreadError_None;
}

ThreadError NetworkData::Remove(uint8_t *aStart, uint8_t aLength)
{
    assert(aLength <= mLength &&
           mTlvs <= aStart &&
           (aStart - mTlvs) + aLength <= mLength);
    memmove(aStart, aStart + aLength, mLength - ((aStart - mTlvs) + aLength));
    mLength -= aLength;
    return kThreadError_None;
}

}  // namespace NetworkData
}  // namespace Thread
