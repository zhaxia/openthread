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

#ifndef HMAC_H_
#define HMAC_H_

#include <stdint.h>
#include <common/thread_error.h>
#include <crypto/hash.h>

namespace Thread {
namespace Crypto {

class Hmac
{
public:
    explicit Hmac(Hash &hash);
    ThreadError SetKey(const void *key, uint16_t key_length);
    ThreadError Init();
    ThreadError Input(const void *buf, uint16_t buf_length);
    ThreadError Finalize(uint8_t *hash);

private:
    enum
    {
        kMaxKeyLength = 64,
    };
    uint8_t m_key[kMaxKeyLength];
    uint8_t m_key_length = 0;
    Hash *m_hash;
};

}  // namespace Crypto
}  // namespace Thread

#endif  // CRYPTO_HMAC_H_
