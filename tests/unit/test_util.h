
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <common/thread_error.hpp>
#include <stdio.h>
#include <stdlib.h>

#define SuccessOrQuit(ERR, MSG)                 \
  do { \
    if ((ERR) != kThreadError_None)     \
    { \
      fprintf(stderr, "%s FAILED: ", __FUNCTION__); \
      fputs(MSG, stderr); \
      exit(-1); \
    } \
  } while (0)

#define VerifyOrQuit(TST, MSG) \
  do { \
    if (!(TST)) \
    { \
      fprintf(stderr, "%s FAILED: ", __FUNCTION__); \
      fputs(MSG, stderr); \
      exit(-1); \
    } \
  } while (0)

#endif
