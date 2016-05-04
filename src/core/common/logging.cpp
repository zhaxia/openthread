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
 *   This file implements the tasklet scheduler.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <platform/logging.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This static method outputs a line of the memory dump.
 *
 * @param[in]  aLogLevel   The log level.
 * @param[in]  aLogRegion  The log region.
 * @param[in]  aBuf        A pointer to the buffer.
 * @param[in]  aLength     Number of bytes in the buffer.
 *
 */
static void DumpLine(otLogLevel aLogLevel, otLogRegion aLogRegion, const void *aBuf, const int aLength)
{
    char buf[80];
    char *cur = buf;

    snprintf(cur, sizeof(buf) - (cur - buf), "|");
    cur += strlen(cur);

    for (int i = 0; i < 16; i++)
    {
        if (i < aLength)
        {
            snprintf(cur, sizeof(buf) - (cur - buf), " %02X", ((uint8_t *)(aBuf))[i]);
            cur += strlen(cur);
        }
        else
        {
            snprintf(cur, sizeof(buf) - (cur - buf), " ..");
            cur += strlen(cur);
        }

        if (!((i + 1) % 8))
        {
            snprintf(cur, sizeof(buf) - (cur - buf), " |");
            cur += strlen(cur);
        }
    }

    snprintf(cur, sizeof(buf) - (cur - buf), " ");
    cur += strlen(cur);

    for (int i = 0; i < 16; i++)
    {
        char c = 0x7f & ((char *)(aBuf))[i];

        if (i < aLength && isprint(c))
        {
            snprintf(cur, sizeof(buf) - (cur - buf), "%c", c);
            cur += strlen(cur);
        }
        else
        {
            snprintf(cur, sizeof(buf) - (cur - buf), ".");
            cur += strlen(cur);
        }
    }

    otLog(aLogLevel, aLogRegion, "%s\n", buf);
}

void otDump(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aId, const void *aBuf, const int aLength)
{
    int idlen = strlen(aId);
    const int width = 72;
    char buf[80];
    char *cur = buf;

    otLog(aLogLevel, aLogRegion, "\n");

    for (int i = 0; i < (width - idlen) / 2 - 5; i++)
    {
        snprintf(cur, sizeof(buf) - (cur - buf), "=");
        cur += strlen(cur);
    }

    snprintf(cur, sizeof(buf) - (cur - buf), "[%s len=%03d]", aId, aLength);
    cur += strlen(cur);

    for (int i = 0; i < (width - idlen) / 2 - 4; i++)
    {
        snprintf(cur, sizeof(buf) - (cur - buf), "=");
        cur += strlen(cur);
    }

    otLog(aLogLevel, aLogRegion, "%s\n", buf);

    for (int i = 0; i < aLength; i += 16)
    {
        DumpLine(aLogLevel, aLogRegion, (uint8_t *)(aBuf) + i, (aLength - i) < 16 ? (aLength - i) : 16);
    }

    cur = buf;

    for (int i = 0; i < width; i++)
    {
        snprintf(cur, sizeof(buf) - (cur - buf), "-");
        cur += strlen(cur);
    }

    otLog(aLogLevel, aLogRegion, "%s\n", buf);
}

#ifdef __cplusplus
};
#endif
