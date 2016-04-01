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

#include <common/code_utils.h>
#include <mac/mac_whitelist.h>

namespace Thread {
namespace Mac {

Whitelist::Whitelist()
{
    for (int i = 0; i < kMaxEntries; i++)
    {
        m_whitelist[i].valid = false;
    }
}

ThreadError Whitelist::Enable()
{
    m_enabled = true;
    return kThreadError_None;
}

ThreadError Whitelist::Disable()
{
    m_enabled = false;
    return kThreadError_None;
}

bool Whitelist::IsEnabled() const
{
    return m_enabled;
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
        if (m_whitelist[i].valid)
        {
            continue;
        }

        memcpy(&m_whitelist[i], &address, sizeof(m_whitelist[i]));
        m_whitelist[i].valid = true;
        m_whitelist[i].rssi_valid = false;
        ExitNow(rval = i);
    }

exit:
    return rval;
}

ThreadError Whitelist::Clear()
{
    for (int i = 0; i < kMaxEntries; i++)
    {
        m_whitelist[i].valid = false;
    }

    return kThreadError_None;
}

ThreadError Whitelist::Remove(const Address64 &address)
{
    ThreadError error = kThreadError_None;
    int i;

    VerifyOrExit((i = Find(address)) >= 0, ;);
    memset(&m_whitelist[i], 0, sizeof(m_whitelist[i]));

exit:
    return error;
}

int Whitelist::Find(const Address64 &address) const
{
    int rval = -1;

    for (int i = 0; i < kMaxEntries; i++)
    {
        if (!m_whitelist[i].valid)
        {
            continue;
        }

        if (memcmp(&m_whitelist[i].addr64, &address, sizeof(m_whitelist[i].addr64)) == 0)
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
    rval = m_whitelist[entry].addr64.bytes;

exit:
    return rval;
}

ThreadError Whitelist::ClearRssi(uint8_t entry)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(entry < kMaxEntries, error = kThreadError_Error);
    m_whitelist[entry].rssi_valid = false;

exit:
    return error;
}

ThreadError Whitelist::GetRssi(uint8_t entry, int8_t &rssi) const
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(entry < kMaxEntries && m_whitelist[entry].valid && m_whitelist[entry].rssi_valid,
                 error = kThreadError_Error);

    rssi = m_whitelist[entry].rssi;

exit:
    return error;
}

ThreadError Whitelist::SetRssi(uint8_t entry, int8_t rssi)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(entry < kMaxEntries, error = kThreadError_Error);
    m_whitelist[entry].rssi_valid = true;
    m_whitelist[entry].rssi = rssi;

exit:
    return error;
}

}  // namespace Mac
}  // namespace Thread
