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

#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#ifndef __APPLE__
#include <time.h>
#endif

#if !defined(AUTOMAKE) && !defined(CPU_POSIX)

#define assert(cond)                            \
  do {                                          \
    if (!(cond)) {                              \
      while (1) {}                              \
    }                                           \
  } while (0)

#define dprintf(...)

#else

#include <assert.h>

inline void dprintf(const char *fmt, ...)
{
    struct timeval tv;
    char time_string[40];

    gettimeofday(&tv, NULL);
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
#ifdef __APPLE__
    printf("%s.%06d ", time_string, tv.tv_usec);
#else
    printf("%s.%06ld ", time_string, tv.tv_usec);
#endif

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

#endif

#ifdef __cplusplus
extern "C" {
#endif

inline void dump_line(const char *id, const void *addr, const int len)
{
    printf("|");

    for (int i = 0; i < 16; i++)
    {
        if (i < len)
        {
            printf(" %02X", ((uint8_t *)(addr))[i]);
        }
        else
        {
            printf(" ..");
        }

        if (!((i + 1) % 8))
        {
            printf(" |");
        }
    }

    printf("\t");

    for (int i = 0; i < 16; i++)
    {
        char c = 0x7f & ((char *)(addr))[i];

        if (i < len && isprint(c))
        {
            printf("%c", c);
        }
        else
        {
            printf(".");
        }
    }
}

inline void dump(const char *id, const void *addr, const int len)
{
    int idlen = strlen(id);
    const int width = 72;

    printf("\n");

    for (int i = 0; i < (width - idlen) / 2 - 5; i++)
    {
        printf("=");
    }

    printf("[%s len=%03d]", id, len);

    for (int i = 0; i < (width - idlen) / 2 - 4; i++)
    {
        printf("=");
    }

    printf("\n");

    for (int i = 0; i < len; i += 16)
    {
        dump_line(id, (uint8_t *)(addr) + i, (len - i) < 16 ? (len - i) : 16);
        printf("\n");
    }

    for (int i = 0; i < width; i++)
    {
        printf("-");
    }

    printf("\n");
}

#ifdef __cplusplus
};
#endif

#endif  // DEBUG_HPP_
