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

#ifndef SHA256_H_
#define SHA256_H_

#include <stdint.h>
#include <common/thread_error.h>
#include <crypto/hash.h>

namespace Thread {
namespace Crypto {

class Sha256: public Hash
{
public:
    uint16_t GetSize() const final;
    ThreadError Init() final;
    ThreadError Input(const void *buf, uint16_t buf_length) final;
    ThreadError Finalize(uint8_t *hash) final;

private:
    void PadMessage();
    void ProcessBlock();

    enum
    {
        kHashSize = 32,
    };
    uint32_t m_hash[kHashSize / sizeof(uint32_t)];
    uint32_t m_length_lo;
    uint32_t m_length_hi;
    uint8_t m_block_index;
    uint8_t m_block[64];
};

}  // namespace Crypto
}  // namespace Thread

#endif  // CRYPTO_SHA256_H_
