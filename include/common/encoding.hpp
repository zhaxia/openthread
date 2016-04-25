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
 *   This file includes definitions for byte-ordering encoding.
 */

#ifndef ENCODING_HPP_
#define ENCODING_HPP_

#include <stdint.h>

namespace Thread {
namespace Encoding {

inline uint16_t Swap16(uint16_t v)
{
    return
        ((v & static_cast<uint16_t>(0x00ffU)) << 8) |
        ((v & static_cast<uint16_t>(0xff00U)) >> 8);
}

inline uint32_t Swap32(uint32_t v)
{
    return
        ((v & static_cast<uint32_t>(0x000000ffUL)) << 24) |
        ((v & static_cast<uint32_t>(0x0000ff00UL)) <<  8) |
        ((v & static_cast<uint32_t>(0x00ff0000UL)) >>  8) |
        ((v & static_cast<uint32_t>(0xff000000UL)) >> 24);
}

inline uint64_t Swap64(uint64_t v)
{
    return
        ((v & static_cast<uint64_t>(0x00000000000000ffULL)) << 56) |
        ((v & static_cast<uint64_t>(0x000000000000ff00ULL)) << 40) |
        ((v & static_cast<uint64_t>(0x0000000000ff0000ULL)) << 24) |
        ((v & static_cast<uint64_t>(0x00000000ff000000ULL)) <<  8) |
        ((v & static_cast<uint64_t>(0x000000ff00000000ULL)) >>  8) |
        ((v & static_cast<uint64_t>(0x0000ff0000000000ULL)) >> 24) |
        ((v & static_cast<uint64_t>(0x00ff000000000000ULL)) >> 40) |
        ((v & static_cast<uint64_t>(0xff00000000000000ULL)) >> 56);
}

namespace BigEndian {

#if BYTE_ORDER_BIG_ENDIAN

inline uint16_t HostSwap16(uint16_t v) { return v; }
inline uint32_t HostSwap32(uint32_t v) { return v; }
inline uint64_t HostSwap64(uint64_t v) { return v; }

#else /* BYTE_ORDER_LITTLE_ENDIAN */

inline uint16_t HostSwap16(uint16_t v) { return Swap16(v); }
inline uint32_t HostSwap32(uint32_t v) { return Swap32(v); }
inline uint64_t HostSwap64(uint64_t v) { return Swap64(v); }

#endif  // LITTLE_ENDIAN

}  // namespace BigEndian

namespace LittleEndian {

#if BYTE_ORDER_BIG_ENDIAN

inline uint16_t HostSwap16(uint16_t v) { return Swap16(v); }
inline uint32_t HostSwap32(uint32_t v) { return Swap32(v); }
inline uint64_t HostSwap64(uint64_t v) { return Swap64(v); }

#else /* BYTE_ORDER_LITTLE_ENDIAN */

inline uint16_t HostSwap16(uint16_t v) { return v; }
inline uint32_t HostSwap32(uint32_t v) { return v; }
inline uint64_t HostSwap64(uint64_t v) { return v; }

#endif

}  // namespace LittleEndian
}  // namespace Encoding
}  // namespace Thread

#endif  // ENCODING_HPP_
