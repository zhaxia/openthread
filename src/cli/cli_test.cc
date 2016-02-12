/**
 * Internal shell tests for assessing platform level abstractions are working:
 *   timer, ...
 *
 *    @file    cli_test.cc
 *    @author  Martin Turon <mturon@nestlabs.com>
 *    @date    August 6, 2015
 *
 *
 *    Copyright (c) 2015 Nest Labs, Inc.  All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 */

#include <cli/cli.h>
#include <cli/cli_test.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace Thread {

static const char kName[] = "test";

static CliServer *server_;

CliTest::CliTest(CliServer *server):
    CliCommand(server),
    timer_(&HandleTimer, this) {
}

const char *CliTest::GetName() {
  return kName;
}

int CliTest::PrintUsage(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = cur + buf_length;

  snprintf(cur, end - cur,
           "usage: test\r\n"
           "  timer  - triggers a 1 sec timer\r\n"
          );
  cur += strlen(cur);

  return cur - buf;
}

/**
 * Triggered by the cli command and then outputs to
 * the shell server 1 second later.
 */
void CliTest::HandleTimer(void *context) {
  CliTest *obj = reinterpret_cast<CliTest*>(context);
  obj->HandleTimer();
}

void CliTest::HandleTimer() {
  char buf[256];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  snprintf(cur, end - cur, "Test timer: fired!\r\n");
  cur += strlen(cur);
  server_->Output(buf, cur - buf);
}

int CliTest::TestTimer(char *buf, uint16_t buf_length) {
  char *cur = buf;
  char *end = buf + buf_length;

  snprintf(cur, end - cur, "Test timer: start 1 sec\r\n");
  cur += strlen(cur);

  timer_.Start(1000);

  return cur - buf;
}

void CliTest::Run(int argc, char *argv[], CliServer *server) {
  char buf[512];
  char *cur = buf;
  char *end = cur + sizeof(buf);

  server_ = server;

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      goto print_usage;

    } else if (strcmp(argv[i], "timer") == 0) {
      cur += TestTimer(buf, sizeof(buf));
      goto done;
    }
  }

print_usage:
  cur += PrintUsage(cur, end - cur);

done:
  snprintf(cur, end - cur, "Done\r\n");
  cur += strlen(cur);
  server->Output(buf, cur - buf);
}

}  // namespace Thread
