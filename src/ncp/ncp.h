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

#ifndef NCP_NCP_H_
#define NCP_NCP_H_

#include <common/message.h>
#include <common/thread_error.h>
#include <ncp/hdlc.h>
#include <ncp/ncp.pb-c.h>
#include <platform/common/uart.h>
#include <thread/thread_netif.h>

namespace Thread {

class Ncp: public Hdlc::Callbacks {
 public:
  Ncp();
  ThreadError Start();
  ThreadError Stop();

  ThreadError SendMessage(Message *message);
  void HandleReceive(uint8_t protocol, uint8_t *buf, uint16_t buf_length) final;
  void HandleSendDone() final;
  void HandleSendMessageDone() final;

 private:
  ThreadError ProcessThreadControl(uint8_t *buf, uint16_t buf_length);
  ThreadError ProcessPrimitive(ThreadControl *message);
  ThreadError ProcessPrimitiveKey(ThreadControl *message);
  ThreadError ProcessPrimitiveKeySequence(ThreadControl *message);
  ThreadError ProcessPrimitiveMeshLocalPrefix(ThreadControl *message);
  ThreadError ProcessPrimitiveMode(ThreadControl *message);
  ThreadError ProcessPrimitiveStatus(ThreadControl *message);
  ThreadError ProcessPrimitiveTimeout(ThreadControl *message);
  ThreadError ProcessPrimitiveChannel(ThreadControl *message);
  ThreadError ProcessPrimitivePanId(ThreadControl *message);
  ThreadError ProcessPrimitiveExtendedPanId(ThreadControl *message);
  ThreadError ProcessPrimitiveNetworkName(ThreadControl *message);
  ThreadError ProcessPrimitiveShortAddr(ThreadControl *message);
  ThreadError ProcessPrimitiveExtAddr(ThreadControl *message);
  ThreadError ProcessState(ThreadControl *message);
  ThreadError ProcessWhitelist(ThreadControl *message);
  ThreadError ProcessScanRequest(ThreadControl *message);

  static void HandleActiveScanResult(void *context, Mac::ActiveScanResult *result);
  void HandleActiveScanResult(Mac::ActiveScanResult *result);

  static void HandleNetifCallback(void *context);
  static void RunUpdateAddressesTask(void *context);

  ThreadNetif netif_;
  NetifCallback netif_callback_;
  Uart uart_;
  Hdlc hdlc_;
  bool hdlc_sending_;
  uint16_t scan_result_index_;

  void RunUpdateAddressesTask();
  Tasklet update_addresses_task_;

  void RunSendScanResultsTask();

  MessageQueue send_queue_;
};

}  // namespace Thread

#endif  // NCP_NCP_H_
