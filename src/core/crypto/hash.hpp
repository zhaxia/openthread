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

#ifndef HASH_HPP_
#define HASH_HPP_

#include <stdint.h>

#include <common/thread_error.hpp>

namespace Thread {
namespace Crypto {

class Hash
{
public:
    virtual uint16_t GetSize() const = 0;
    virtual ThreadError Init() = 0;
    virtual ThreadError Input(const void *buf, uint16_t bufLength) = 0;
    virtual ThreadError Finalize(uint8_t *hash) = 0;
};

}  // namespace Crypto
}  // namespace Thread

#endif  // HASH_HPP_
