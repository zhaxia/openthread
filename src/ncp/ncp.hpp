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

#ifndef NCP_NCP_HPP_
#define NCP_NCP_HPP_

#include <ncp/ncp_base.hpp>
#include <ncp/hdlc.hpp>

namespace Thread {

class Ncp : public NcpBase
{
    typedef NcpBase super_t;

public:
    Ncp();

    ThreadError Init() final;
    ThreadError Start() final;
    ThreadError Stop() final;

    ThreadError SendMessage(uint8_t protocol, Message &message) final;
    ThreadError Send(uint8_t protocol, uint8_t *frame,
                     uint16_t frameLength) final;

private:
    Hdlc mHdlc;
};

}  // namespace Thread

#endif  // NCP_NCP_HPP_
