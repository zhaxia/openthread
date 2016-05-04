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

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <platform/logging.h>

void otLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    struct timeval tv;
    char timeString[40];
    va_list args;

    gettimeofday(&tv, NULL);
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
    printf("%s.%06d ", timeString, (uint32_t)tv.tv_usec);

    switch (aLogLevel)
    {
    case kLogLevelNone:
        printf("NONE ");
        break;

    case kLogLevelCrit:
        printf("CRIT ");
        break;

    case kLogLevelWarn:
        printf("WARN ");
        break;

    case kLogLevelInfo:
        printf("INFO ");
        break;

    case kLogLevelDebg:
        printf("DEBG ");
        break;
    }

    switch (aLogRegion)
    {
    case kLogRegionApi:
        printf("API  ");
        break;

    case kLogRegionMle:
        printf("MLE  ");
        break;

    case kLogRegionArp:
        printf("ARP  ");
        break;

    case kLogRegionNetData:
        printf("NETD ");
        break;

    case kLogRegionIp6:
        printf("IPV6 ");
        break;

    case kLogRegionIcmp:
        printf("ICMP ");
        break;

    case kLogRegionMac:
        printf("MAC  ");
        break;

    case kLogRegionMem:
        printf("MEM  ");
        break;
    }

    va_start(args, aFormat);
    vprintf(aFormat, args);
    va_end(args);
}

