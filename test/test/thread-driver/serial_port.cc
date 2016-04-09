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

#include <cmdline.h>
#include <serial_port.h>
#include <common/code_utils.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct gengetopt_args_info args_info;
extern void uart_handle_send_done();
extern void uart_handle_receive(uint8_t *buf, uint16_t buf_length);
int fd_;

ThreadError uart_start()
{
    ThreadError error = kThreadError_None;
    struct termios termios;

    // open file
    VerifyOrExit((fd_ = open(args_info.tty_arg, O_RDWR | O_NOCTTY)) >= 0,
                 perror(args_info.tty_arg); error = kThreadError_Error);

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

    return error;

exit:
    close(fd_);
    return error;
}

ThreadError uart_stop()
{
    ThreadError error = kThreadError_None;
    VerifyOrExit(close(fd_) == 0, perror("close"); error = kThreadError_Error);
exit:
    return error;
}

ThreadError uart_send(const uint8_t *buf, uint16_t buf_length)
{
    write(fd_, buf, buf_length);
    uart_handle_send_done();
    return kThreadError_None;
}

int uart_get_fd()
{
    return fd_;
}

ThreadError uart_read()
{
    ThreadError error = kThreadError_None;
    uint8_t buf[1024];
    size_t length;

    VerifyOrExit((length = read(fd_, buf, sizeof(buf))) >= 0, error = kThreadError_Error);
    uart_handle_receive(buf, length);

exit:
    return error;
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
