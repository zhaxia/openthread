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
 *   This file implements whitelist IEEE 802.15.4 frame filtering based on MAC address.
 */

#include <string.h>

#include <common/code_utils.hpp>
#include <mac/mac_whitelist.hpp>

namespace Thread {
namespace Mac {

void Whitelist::Init(void)
{
    mEnabled = false;

    for (int i = 0; i < kMaxEntries; i++)
    {
        mWhitelist[i].mValid = false;
    }
}

void Whitelist::Enable(void)
{
    mEnabled = true;
}

void Whitelist::Disable(void)
{
    mEnabled = false;
}

bool Whitelist::IsEnabled(void) const
{
    return mEnabled;
}

int Whitelist::GetMaxEntries(void) const
{
    return kMaxEntries;
}

const Whitelist::Entry *Whitelist::GetEntries(void) const
{
    return mWhitelist;
}

Whitelist::Entry *Whitelist::Add(const ExtAddress &address)
{
    Entry *rval;

    VerifyOrExit((rval = Find(address)) == NULL, ;);

    for (int i = 0; i < kMaxEntries; i++)
    {
        if (mWhitelist[i].mValid)
        {
            continue;
        }

        memcpy(&mWhitelist[i], &address, sizeof(mWhitelist[i]));
        mWhitelist[i].mValid = true;
        mWhitelist[i].mConstantRssi = false;
        ExitNow(rval = &mWhitelist[i]);
    }

exit:
    return rval;
}

void Whitelist::Clear(void)
{
    for (int i = 0; i < kMaxEntries; i++)
    {
        mWhitelist[i].mValid = false;
    }
}

void Whitelist::Remove(const ExtAddress &address)
{
    Entry *entry;

    VerifyOrExit((entry = Find(address)) != NULL, ;);
    memset(entry, 0, sizeof(*entry));

exit:
    {}
}

Whitelist::Entry *Whitelist::Find(const ExtAddress &address)
{
    Entry *rval = NULL;

    for (int i = 0; i < kMaxEntries; i++)
    {
        if (!mWhitelist[i].mValid)
        {
            continue;
        }

        if (memcmp(&mWhitelist[i].mExtAddress, &address, sizeof(mWhitelist[i].mExtAddress)) == 0)
        {
            ExitNow(rval = &mWhitelist[i]);
        }
    }

exit:
    return rval;
}

void Whitelist::ClearConstantRssi(Entry &aEntry)
{
    aEntry.mConstantRssi = false;
}

ThreadError Whitelist::GetConstantRssi(Entry &aEntry, int8_t &rssi) const
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(aEntry.mValid && aEntry.mConstantRssi, error = kThreadError_Error);
    rssi = aEntry.mRssi;

exit:
    return error;
}

void Whitelist::SetConstantRssi(Entry &aEntry, int8_t aRssi)
{
    aEntry.mConstantRssi = true;
    aEntry.mRssi = aRssi;
}

}  // namespace Mac
}  // namespace Thread
