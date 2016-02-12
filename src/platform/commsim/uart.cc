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

#include <platform/posix/cmdline.h>
#include <platform/posix/uart.h>
#include <common/code_utils.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

////extern struct gengetopt_args_info args_info;

namespace Thread {

Uart::Uart(Callbacks *callbacks):
    UartInterface(callbacks),
    receive_task_(&ReceiveTask, this),
    send_task_(&SendTask, this) {
}

ThreadError Uart::Start() {
  ThreadError error = kThreadError_None;
  struct termios termios;

  // open file
#if 0
  VerifyOrExit((fd_ = posix_openpt(O_RDWR | O_NOCTTY)) >= 0, perror("posix_openpt"); error = kThreadError_Error);
  VerifyOrExit(grantpt(fd_) == 0, perror("grantpt"); error = kThreadError_Error);
  VerifyOrExit(unlockpt(fd_) == 0, perror("unlockpt"); error = kThreadError_Error);

  // print pty path
  char *path;
  VerifyOrExit((path = ptsname(fd_)) != NULL, perror("ptsname"); error = kThreadError_Error);
  printf("%s\n", path);
  free(path);
#else
  char *path;
////  asprintf(&path, "/dev/ptyp%d", args_info.eui64_arg);
  asprintf(&path, "/dev/ptyp%d", 1);
  VerifyOrExit((fd_ = open(path, O_RDWR | O_NOCTTY)) >= 0, perror("posix_openpt"); error = kThreadError_Error);
  free(path);

  // print pty path
////  printf("/dev/ttyp%d\n", args_info.eui64_arg);
  printf("/dev/ttyp%d\n", 0);
#endif

  // check if file descriptor is pointing to a TTY device
  VerifyOrExit(isatty(fd_), error = kThreadError_Error);

  // get current configuration
  VerifyOrExit(tcgetattr(fd_, &termios) == 0, perror("tcgetattr"); error = kThreadError_Error);

  // turn off input processing
  termios.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);

  // turn off output processing
  termios.c_oflag = 0;

  // turn off line processing
  termios.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

  // turn off character processing
  termios.c_cflag &= ~(CSIZE | PARENB);
  termios.c_cflag |= CS8;

  // return 1 byte at a time
  termios.c_cc[VMIN]  = 1;

  // turn off inter-character timer
  termios.c_cc[VTIME] = 0;

  // configure baud rate
  VerifyOrExit(cfsetispeed(&termios, B115200) == 0, perror("cfsetispeed"); error = kThreadError_Error);
  VerifyOrExit(cfsetospeed(&termios, B115200) == 0, perror("cfsetispeed"); error = kThreadError_Error);

  // set configuration
  VerifyOrExit(tcsetattr(fd_, TCSAFLUSH, &termios) == 0, perror("tcsetattr"); error = kThreadError_Error);

  char cmd[256];
////  snprintf(cmd, sizeof(cmd), "thread_uart_semaphore_%d", args_info.eui64_arg);
  snprintf(cmd, sizeof(cmd), "thread_uart_semaphore_%d", 0);
  semaphore_ = sem_open(cmd, O_CREAT, 0644, 0);
  pthread_create(&thread_, NULL, ReceiveThread, this);

  return error;

exit:
  close(fd_);
  return error;
}

ThreadError Uart::Stop() {
  ThreadError error = kThreadError_None;

  close(fd_);

  return error;
}

ThreadError Uart::Send(const uint8_t *buf, uint16_t buf_length) {
  ThreadError error = kThreadError_None;

  VerifyOrExit(write(fd_, buf, buf_length) >= 0, error = kThreadError_Error);
  send_task_.Post();

exit:
  return error;
}

void Uart::SendTask(void *context) {
  Uart *obj = reinterpret_cast<Uart*>(context);
  obj->SendTask();
}

void Uart::SendTask() {
  callbacks_->HandleSendDone();
}

void *Uart::ReceiveThread(void *arg) {
  Uart *obj = reinterpret_cast<Uart*>(arg);

  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(obj->fd_, &fds);

    int rval = select(obj->fd_ + 1, &fds, NULL, NULL, NULL);
    if (rval >= 0 && FD_ISSET(obj->fd_, &fds)) {
      obj->receive_task_.Post();
      sem_wait(obj->semaphore_);
    }
  }

  return NULL;
}

void Uart::ReceiveTask(void *context) {
  Uart *obj = reinterpret_cast<Uart*>(context);
  obj->ReceiveTask();
}

void Uart::ReceiveTask() {
  uint8_t receive_buffer[1024];
  size_t len;

  len = read(fd_, receive_buffer, sizeof(receive_buffer));
  callbacks_->HandleReceive(receive_buffer, len);

  sem_post(semaphore_);
}

}  // namespace Thread
