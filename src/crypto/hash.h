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

#ifndef HASH_H_
#define HASH_H_

#include <stdint.h>
#include <common/thread_error.h>

namespace Thread {
namespace Crypto {

class Hash
{
public:
    virtual uint16_t GetSize() const = 0;
    virtual ThreadError Init() = 0;
    virtual ThreadError Input(const void *buf, uint16_t buf_length) = 0;
    virtual ThreadError Finalize(uint8_t *hash) = 0;
};

}  // namespace Crypto
}  // namespace Thread

#endif  // HASH_H_
