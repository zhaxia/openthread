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

#include <common/code_utils.hpp>
#include <mac/mac_whitelist.hpp>

namespace Thread {
namespace Mac {

Whitelist::Whitelist()
{
    for (int i = 0; i < kMaxEntries; i++)
    {
        mWhitelist[i].mValid = false;
    }
}

ThreadError Whitelist::Enable()
{
    mEnabled = true;
    return kThreadError_None;
}

ThreadError Whitelist::Disable()
{
    mEnabled = false;
    return kThreadError_None;
}

bool Whitelist::IsEnabled() const
{
    return mEnabled;
}

int Whitelist::GetMaxEntries() const
{
    return kMaxEntries;
}

int Whitelist::Add(const Address64 &address)
{
    int rval = -1;

    VerifyOrExit((rval = Find(address)) < 0, ;);

    for (int i = 0; i < kMaxEntries; i++)
    {
        if (mWhitelist[i].mValid)
        {
            continue;
        }

        memcpy(&mWhitelist[i], &address, sizeof(mWhitelist[i]));
        mWhitelist[i].mValid = true;
        mWhitelist[i].mRssiValid = false;
        ExitNow(rval = i);
    }

exit:
    return rval;
}

ThreadError Whitelist::Clear()
{
    for (int i = 0; i < kMaxEntries; i++)
    {
        mWhitelist[i].mValid = false;
    }

    return kThreadError_None;
}

ThreadError Whitelist::Remove(const Address64 &address)
{
    ThreadError error = kThreadError_None;
    int i;

    VerifyOrExit((i = Find(address)) >= 0, ;);
    memset(&mWhitelist[i], 0, sizeof(mWhitelist[i]));

exit:
    return error;
}

int Whitelist::Find(const Address64 &address) const
{
    int rval = -1;

    for (int i = 0; i < kMaxEntries; i++)
    {
        if (!mWhitelist[i].mValid)
        {
            continue;
        }

        if (memcmp(&mWhitelist[i].mAddr64, &address, sizeof(mWhitelist[i].mAddr64)) == 0)
        {
            ExitNow(rval = i);
        }
    }

exit:
    return rval;
}

const uint8_t *Whitelist::GetAddress(uint8_t entry) const
{
    const uint8_t *rval;

    VerifyOrExit(entry < kMaxEntries, rval = NULL);
    rval = mWhitelist[entry].mAddr64.mBytes;

exit:
    return rval;
}

ThreadError Whitelist::ClearRssi(uint8_t entry)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(entry < kMaxEntries, error = kThreadError_Error);
    mWhitelist[entry].mRssiValid = false;

exit:
    return error;
}

ThreadError Whitelist::GetRssi(uint8_t entry, int8_t &rssi) const
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(entry < kMaxEntries && mWhitelist[entry].mValid && mWhitelist[entry].mRssiValid,
                 error = kThreadError_Error);

    rssi = mWhitelist[entry].mRssi;

exit:
    return error;
}

ThreadError Whitelist::SetRssi(uint8_t entry, int8_t rssi)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(entry < kMaxEntries, error = kThreadError_Error);
    mWhitelist[entry].mRssiValid = true;
    mWhitelist[entry].mRssi = rssi;

exit:
    return error;
}

}  // namespace Mac
}  // namespace Thread
