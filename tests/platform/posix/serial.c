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

#include <pthread.h>
#include <semaphore.h>
#include <termios.h>
#include <unistd.h>
// Linux build needs these
#include <stdlib.h>
#include <fcntl.h>

#include <platform/posix/cmdline.h>
#include <platform/serial.h>
#include <common/code_utils.hpp>

static void *serial_receive_thread(void *arg);

extern struct gengetopt_args_info args_info;

static int s_fd;
static pthread_t s_pthread;
static sem_t *s_semaphore;

ThreadError ot_serial_enable()
{
    ThreadError error = kThreadError_None;
    struct termios termios;
    char *path;
    char cmd[256];
    printf("here\n");
    // open file
#if __APPLE__

    asprintf(&path, "/dev/ptyp%d", args_info.nodeid_arg);
    VerifyOrExit((s_fd = open(path, O_RDWR | O_NOCTTY)) >= 0, perror("posix_openpt"); error = kThreadError_Error);
    free(path);

    // print pty path
    printf("/dev/ttyp%d\n", args_info.nodeid_arg);

#else

    VerifyOrExit((s_fd = posix_openpt(O_RDWR | O_NOCTTY)) >= 0, perror("posix_openpt"); error = kThreadError_Error);
    VerifyOrExit(grantpt(s_fd) == 0, perror("grantpt"); error = kThreadError_Error);
    VerifyOrExit(unlockpt(s_fd) == 0, perror("unlockpt"); error = kThreadError_Error);

    // print pty path
    VerifyOrExit((path = ptsname(s_fd)) != NULL, perror("ptsname"); error = kThreadError_Error);
    printf("%s\n", path);
    free(path);
#endif

    // check if file descriptor is pointing to a TTY device
    VerifyOrExit(isatty(s_fd), error = kThreadError_Error);

    // get current configuration
    VerifyOrExit(tcgetattr(s_fd, &termios) == 0, perror("tcgetattr"); error = kThreadError_Error);

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
    VerifyOrExit(tcsetattr(s_fd, TCSAFLUSH, &termios) == 0, perror("tcsetattr"); error = kThreadError_Error);

    snprintf(cmd, sizeof(cmd), "thread_serial_semaphore_%d", args_info.nodeid_arg);
    s_semaphore = sem_open(cmd, O_CREAT, 0644, 0);
    pthread_create(&s_pthread, NULL, &serial_receive_thread, NULL);

    return error;

exit:
    close(s_fd);
    return error;
}

ThreadError ot_serial_disable()
{
    ThreadError error = kThreadError_None;

    close(s_fd);

    return error;
}

ThreadError ot_serial_send(const uint8_t *buf, uint16_t buf_length)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(write(s_fd, buf, buf_length) >= 0, error = kThreadError_Error);
    ot_serial_signal_send_done();

exit:
    return error;
}

void ot_serial_handle_send_done(void)
{
}

void *serial_receive_thread(void *arg)
{
    fd_set fds;
    int rval;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(s_fd, &fds);

        rval = select(s_fd + 1, &fds, NULL, NULL, NULL);

        if (rval >= 0 && FD_ISSET(s_fd, &fds))
        {
            ot_serial_signal_receive();
            sem_wait(s_semaphore);
        }
    }

    return NULL;
}

static uint8_t sReceiveBuffer[128];

const uint8_t *ot_serial_get_received_bytes(uint16_t *aBufLength)
{
    size_t length;

    length = read(s_fd, sReceiveBuffer, sizeof(sReceiveBuffer));

    if (aBufLength != NULL)
    {
        *aBufLength = length;
    }

    return sReceiveBuffer;
}

void ot_serial_handle_receive_done()
{
    sem_post(s_semaphore);
}
