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

#ifndef AES_CCM_H_
#define AES_CCM_H_

#include <stdint.h>
#include <common/thread_error.h>
#include <crypto/aes_ecb.h>

namespace Thread {
namespace Crypto {

class AesCcm
{
public:
    void Init(const AesEcb &ecb, uint32_t header_length, uint32_t plaintext_length, uint8_t tag_length,
              const void *nonce, uint8_t nonce_length);
    void Header(const void *header, uint32_t header_length);
    void Payload(void *plaintext, void *ciphertext, uint32_t length, bool encrypt);
    void Finalize(void *tag, uint8_t *tag_length);

private:
    const AesEcb *m_ecb;
    uint8_t m_block[16];
    uint8_t m_ctr[16];
    uint8_t m_ctrpad[16];
    uint8_t m_nonce_length;
    uint32_t m_header_length;
    uint32_t m_header_cur;
    uint32_t m_plaintext_length;
    uint32_t m_plaintext_cur;
    uint16_t m_block_length;
    uint16_t m_ctr_length;
    uint8_t m_tag_length;
};

}  // namespace Crypto
}  // namespace Thread

#endif  // AES_CCM_H_
