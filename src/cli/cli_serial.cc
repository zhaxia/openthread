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

#include <cli/cli_command.h>
#include <cli/cli_serial.h>
#include <common/code_utils.h>
#include <common/encoding.h>
#include <stdio.h>
#include <string.h>

namespace Thread {

static const uint8_t VT102_ERASE_EOL[] = "\033[K";
static const uint8_t CRNL[] = "\r\n";

CliServerSerial::CliServerSerial():
    uart_(this) {
}

ThreadError CliServerSerial::Add(CliCommand *command) {
  ThreadError error = kThreadError_None;
  CliCommand *cur_command;

  VerifyOrExit(command->next_ == NULL, error = kThreadError_Busy);

  if (commands_ == NULL) {
    commands_ = command;
  } else {
    for (cur_command = commands_; cur_command->next_; cur_command = cur_command->next_) {}
    cur_command->next_ = command;
  }

exit:
  return error;
}

ThreadError CliServerSerial::Start(uint16_t port) {
  rx_length_ = 0;
  uart_.Start();
  return kThreadError_None;
}

void CliServerSerial::HandleReceive(uint8_t *buf, uint16_t buf_length) {
  uint8_t *cur = buf;
  uint8_t *end = cur + buf_length;

  for ( ; cur < end; cur++) {
    switch (*cur) {
      case '\r':
        uart_.Send(CRNL, sizeof(CRNL));
        break;
      default:
        uart_.Send(cur, 1);
        break;
    }
  }

  while (buf_length > 0 && rx_length_ < kRxBufferSize) {
    switch (*buf) {
      case '\r':
      case '\n':
        if (rx_length_ > 0) {
          rx_buffer_[rx_length_] = '\0';
          ProcessCommand();
        }
        break;

      case '\b':
      case 127:
        if (rx_length_ > 0) {
          rx_buffer_[--rx_length_] = '\0';
          uart_.Send(VT102_ERASE_EOL, sizeof(VT102_ERASE_EOL));
        }
        break;

      default:
        rx_buffer_[rx_length_++] = *buf;
        break;
    }

    buf++;
    buf_length--;
  }
}

void CliServerSerial::HandleSendDone() {
}

ThreadError CliServerSerial::ProcessCommand() {
  ThreadError error = kThreadError_None;
  uint16_t payload_length = rx_length_;
  char *cmd;
  char *last;

  if (rx_buffer_[payload_length-1] == '\n')
    rx_buffer_[--payload_length] = '\0';
  if (rx_buffer_[payload_length-1] == '\r')
    rx_buffer_[--payload_length] = '\0';

  VerifyOrExit((cmd = strtok_r(rx_buffer_, " ", &last)) != NULL, ;);

  if (strncmp(cmd, "?", 1) == 0) {
    char *cur = rx_buffer_;
    char *end = cur + sizeof(rx_buffer_);

    snprintf(cur, end - cur, "%s", "Commands:\r\n");
    cur += strlen(cur);
    for (CliCommand *command = commands_; command; command = command->next_) {
      snprintf(cur, end - cur, "%s\r\n", command->GetName());
      cur += strlen(cur);
    }

    SuccessOrExit(error = uart_.Send(reinterpret_cast<const uint8_t *>(rx_buffer_), cur - rx_buffer_));
  } else {
    int argc;
    char *argv[kMaxArgs];

    for (argc = 0; argc < kMaxArgs; argc++) {
      if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL)
        break;
    }

    for (CliCommand *command = commands_; command; command = command->next_) {
      if (strcmp(cmd, command->GetName()) == 0) {
        command->Run(argc, argv, this);
        break;
      }
    }
  }

exit:
  rx_length_ = 0;

  return error;
}

ThreadError CliServerSerial::Output(const char *buf, uint16_t buf_length) {
  ThreadError error = kThreadError_None;

  SuccessOrExit(error = uart_.Send(reinterpret_cast<const uint8_t *>(buf), buf_length));

exit:
  return error;
}

}  // namespace Thread
