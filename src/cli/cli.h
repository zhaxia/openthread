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

#ifndef CLI_CLI_H_
#define CLI_CLI_H_

#if defined(AUTOMAKE)
#include <platform/posix/cli_posix.h>
#else
#include <cli/cli_serial.h>
#endif

#include <common/code_utils.h>

namespace Thread {

static int hex2bin(const char *hex, uint8_t *bin, uint16_t bin_length) {
  uint16_t hex_length = strlen(hex);
  const char *hex_end = hex + hex_length;
  uint8_t *cur = bin;
  uint8_t num_chars = hex_length & 1;
  uint8_t byte = 0;

  if ((hex_length + 1) / 2 > bin_length)
    return -1;

  while (hex < hex_end) {
    if ('A' <= *hex && *hex <= 'F')
      byte |= 10 + (*hex - 'A');
    else if ('a' <= *hex && *hex <= 'f')
      byte |= 10 + (*hex - 'a');
    else if ('0' <= *hex && *hex <= '9')
      byte |= *hex - '0';
    else
      return -1;
    hex++;
    num_chars++;

    if (num_chars >= 2) {
      num_chars = 0;
      *cur++ = byte;
      byte = 0;
    } else {
      byte <<= 4;
    }
  }

  return cur - bin;
}

}  // namespace Thread

#endif  // CLI_CLI_H_
