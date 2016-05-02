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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <cmdline.h>

#include <serial_port.hpp>
#include <common/code_utils.hpp>

extern struct gengetopt_args_info args_info;

namespace Thread {

extern void serial_handle_receive(uint8_t *buf, uint16_t buf_length);
int fd_;

ThreadError serial_enable()
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

ThreadError serial_disable()
{
    ThreadError error = kThreadError_None;
    VerifyOrExit(close(fd_) == 0, perror("close"); error = kThreadError_Error);
exit:
    return error;
}

ThreadError serial_send(const uint8_t *buf, uint16_t buf_length)
{
    write(fd_, buf, buf_length);
    return kThreadError_None;
}

int serial_get_fd()
{
    return fd_;
}

ThreadError serial_read(uint8_t *buf, uint16_t &buf_length)
{
    ThreadError error = kThreadError_None;
    int length;

    VerifyOrExit((length = read(fd_, buf, buf_length)) >= 0, error = kThreadError_Error);
    buf_length = length;

exit:
    return error;
}

}  // namespace Thread
