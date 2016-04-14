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

#ifndef NCP_NCP_BASE_HPP_
#define NCP_NCP_BASE_HPP_

#include <ncp/ncp.pb-c.h>

#include <common/message.hpp>
#include <common/thread_error.hpp>
#include <platform/uart.hpp>
#include <thread/thread_netif.hpp>

enum
{
    kNcpChannel_ThreadControl = 0,    ///< Host initiated Thread control
    kNcpChannel_ThreadInterface = 1,  ///< Unsolicited Thread interface events
    kNcpChannel_ThreadData = 2,       ///< IPv6 data
    kNcpChannel_ThreadBle = 3         ///< BLE HCI
};

namespace Thread {

class NcpBase
{
public:
    NcpBase();

    virtual ThreadError Init();
    virtual ThreadError Start();
    virtual ThreadError Stop();

    virtual ThreadError SendMessage(uint8_t protocol, Message &message) = 0;
    virtual ThreadError Send(uint8_t protocol, uint8_t *frame,
                             uint16_t frame_length) = 0;

    static void HandleReceivedDatagram(void *context, Message &message);
    void HandleReceivedDatagram(Message &message);

protected:
    static void HandleReceive(void *context, uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    void HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length);
    static void HandleSendDone(void *context);
    void HandleSendDone();
    static void HandleSendMessageDone(void *context);
    void HandleSendMessageDone();

private:
    ThreadError ProcessThreadControl(uint8_t *buf, uint16_t buf_length);
    ThreadError ProcessPrimitive(ThreadControl &message);
    ThreadError ProcessPrimitiveKey(ThreadControl &message);
    ThreadError ProcessPrimitiveKeySequence(ThreadControl &message);
    ThreadError ProcessPrimitiveMeshLocalPrefix(ThreadControl &message);
    ThreadError ProcessPrimitiveMode(ThreadControl &message);
    ThreadError ProcessPrimitiveStatus(ThreadControl &message);
    ThreadError ProcessPrimitiveTimeout(ThreadControl &message);
    ThreadError ProcessPrimitiveChannel(ThreadControl &message);
    ThreadError ProcessPrimitivePanId(ThreadControl &message);
    ThreadError ProcessPrimitiveExtendedPanId(ThreadControl &message);
    ThreadError ProcessPrimitiveNetworkName(ThreadControl &message);
    ThreadError ProcessPrimitiveShortAddr(ThreadControl &message);
    ThreadError ProcessPrimitiveExtAddr(ThreadControl &message);
    ThreadError ProcessState(ThreadControl &message);
    ThreadError ProcessWhitelist(ThreadControl &message);
    ThreadError ProcessScanRequest(ThreadControl &message);

    static void HandleActiveScanResult(void *context, Mac::ActiveScanResult *result);
    void HandleActiveScanResult(Mac::ActiveScanResult *result);
    static void HandleUnicastAddressesChanged(void *context);

    static void RunUpdateAddressesTask(void *context);

    ThreadNetif m_netif;
    NetifHandler m_netif_handler;

    bool m_sending;

    uint16_t m_scan_result_index;

    void RunUpdateAddressesTask();
    Tasklet m_update_addresses_task;

    void RunSendScanResultsTask();

protected:
    MessageQueue m_send_queue;
};

}  // namespace Thread

#endif  // NCP_NCP_BASE_HPP_
