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

#ifndef COMMON_DEBUG_H_
#define COMMON_DEBUG_H_

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

#if !defined(AUTOMAKE) and !defined(CPU_POSIX)

#define assert(cond)                            \
  do {                                          \
    if (!(cond)) {                              \
      while (1) {}                              \
    }                                           \
  } while (0)

#define dprintf(...)

#else

#include <assert.h>

inline void dprintf(const char *fmt, ...) {
  struct timeval tv;
  char time_string[40];

  gettimeofday(&tv, NULL);
  strftime (time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
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

namespace Thread {

inline void dump_line(const char *id, const void *addr, const int len) {
  printf("|");
  for (int i = 0; i < 16; i++) {
    if (i < len)
      printf(" %02X", (reinterpret_cast<const uint8_t*>(addr))[i]);
    else
      printf(" ..");
    if (!((i+1) % 8)) {
      printf(" |");
    }
  }
  printf("\t");
  for (int i = 0; i < 16; i++) {
    char c = 0x7f & (reinterpret_cast<const char*>(addr))[i];
    if (i < len && isprint(c))
      printf("%c", c);
    else
      printf(".");
  }
}

inline void dump(const char *id, const void *addr, const int len) {
  int idlen = strlen(id);
  const int width = 72;

  printf("\n");
  for (int i = 0; i < (width - idlen) / 2 - 5; i++)
    printf("=");
  printf("[%s len=%03d]", id, len);
  for (int i = 0; i < (width - idlen) / 2 - 4; i++)
    printf("=");
  printf("\n");
  for (int i = 0; i < len; i += 16) {
    dump_line(id, reinterpret_cast<const uint8_t*>(addr) + i, (len - i) < 16 ? (len - i) : 16);
    printf("\n");
  }
  for (int i = 0; i < width; i++) {
    printf("-");
  }
  printf("\n");
}

}  // namespace Thread

#endif  // COMMON_DEBUG_H_
