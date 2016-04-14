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

#ifndef CLI_HPP_
#define CLI_HPP_

#include <stdint.h>
#include <string.h>

namespace Thread {
namespace Cli {

static int hex2bin(const char *hex, uint8_t *bin, uint16_t bin_length)
{
    uint16_t hex_length = strlen(hex);
    const char *hex_end = hex + hex_length;
    uint8_t *cur = bin;
    uint8_t num_chars = hex_length & 1;
    uint8_t byte = 0;

    if ((hex_length + 1) / 2 > bin_length)
    {
        return -1;
    }

    while (hex < hex_end)
    {
        if ('A' <= *hex && *hex <= 'F')
        {
            byte |= 10 + (*hex - 'A');
        }
        else if ('a' <= *hex && *hex <= 'f')
        {
            byte |= 10 + (*hex - 'a');
        }
        else if ('0' <= *hex && *hex <= '9')
        {
            byte |= *hex - '0';
        }
        else
        {
            return -1;
        }

        hex++;
        num_chars++;

        if (num_chars >= 2)
        {
            num_chars = 0;
            *cur++ = byte;
            byte = 0;
        }
        else
        {
            byte <<= 4;
        }
    }

    return cur - bin;
}

}  // namespace Cli
}  // namespace Thread

#endif  // CLI_HPP_
