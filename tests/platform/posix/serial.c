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

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <common/code_utils.hpp>
#include <platform/posix/cmdline.h>
#include <platform/serial.h>

static void *serial_receive_thread(void *arg);

#ifdef OPENTHREAD_TARGET_LINUX
int posix_openpt(int oflag);
int grantpt(int fildes);
int unlockpt(int fd);
char *ptsname(int fd);
#endif  // OPENTHREAD_TARGET_LINUX

extern struct gengetopt_args_info args_info;

static uint8_t s_receive_buffer[128];
static int s_in_fd;
static int s_out_fd;
static struct termios s_in_termios;
static struct termios s_out_termios;
static pthread_t s_pthread;
static sem_t *s_semaphore;

ThreadError otSerialEnable(void)
{
    ThreadError error = kThreadError_None;
    struct termios termios;
    char *path;
    char cmd[256];

    if (args_info.stdserial_given == 1)
    {
        s_in_fd = dup(STDIN_FILENO);
        s_out_fd = dup(STDOUT_FILENO);
        dup2(STDERR_FILENO, STDOUT_FILENO);
    }
    else
    {
        // open file
#ifdef OPENTHREAD_TARGET_DARWIN

        asprintf(&path, "/dev/ptyp%d", args_info.nodeid_arg);
        VerifyOrExit((s_in_fd = open(path, O_RDWR | O_NOCTTY)) >= 0, perror("posix_openpt"); error = kThreadError_Error);
        free(path);

        // print pty path
        printf("/dev/ttyp%d\n", args_info.nodeid_arg);

#elif defined(OPENTHREAD_TARGET_LINUX)

        VerifyOrExit((s_in_fd = posix_openpt(O_RDWR | O_NOCTTY)) >= 0, perror("posix_openpt"); error = kThreadError_Error);
        VerifyOrExit(grantpt(s_in_fd) == 0, perror("grantpt"); error = kThreadError_Error);
        VerifyOrExit(unlockpt(s_in_fd) == 0, perror("unlockpt"); error = kThreadError_Error);

        // print pty path
        VerifyOrExit((path = ptsname(s_in_fd)) != NULL, perror("ptsname"); error = kThreadError_Error);
        printf("%s\n", path);

#else
#error "Unknown platform."
#endif

        // check if file descriptor is pointing to a TTY device
        VerifyOrExit(isatty(s_in_fd), error = kThreadError_Error);

        s_out_fd = dup(s_in_fd);
    }

    if (isatty(s_in_fd))
    {
        // get current configuration
        VerifyOrExit(tcgetattr(s_in_fd, &termios) == 0, perror("tcgetattr"); error = kThreadError_Error);
        s_in_termios = termios;

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

        // set configuration
        VerifyOrExit(tcsetattr(s_in_fd, TCSAFLUSH, &termios) == 0, perror("tcsetattr"); error = kThreadError_Error);
    }

    if (isatty(s_out_fd))
    {
        // get current configuration
        VerifyOrExit(tcgetattr(s_out_fd, &termios) == 0, perror("tcgetattr"); error = kThreadError_Error);
        s_out_termios = termios;

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
        VerifyOrExit(cfsetospeed(&termios, B115200) == 0, perror("cfsetospeed"); error = kThreadError_Error);

        // set configuration
        VerifyOrExit(tcsetattr(s_out_fd, TCSAFLUSH, &termios) == 0, perror("tcsetattr"); error = kThreadError_Error);
    }

    snprintf(cmd, sizeof(cmd), "thread_serial_semaphore_%d", args_info.nodeid_arg);
    s_semaphore = sem_open(cmd, O_CREAT, 0644, 0);
    pthread_create(&s_pthread, NULL, &serial_receive_thread, NULL);

    return error;

exit:
    close(s_in_fd);
    close(s_out_fd);
    return error;
}

ThreadError otSerialDisable(void)
{
    ThreadError error = kThreadError_None;

    tcsetattr(s_out_fd, TCSAFLUSH, &s_out_termios);
    tcsetattr(s_in_fd, TCSAFLUSH, &s_in_termios);
    close(s_in_fd);
    close(s_out_fd);

    return error;
}

ThreadError otSerialSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(write(s_out_fd, aBuf, aBufLength) >= 0, error = kThreadError_Error);
    otSerialSignalSendDone();

exit:
    return error;
}

void otSerialHandleSendDone(void)
{
}

void *serial_receive_thread(void *aContext)
{
    fd_set fds;
    int rval;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(s_in_fd, &fds);

        rval = select(s_in_fd + 1, &fds, NULL, NULL, NULL);

        if (rval >= 0 && FD_ISSET(s_in_fd, &fds))
        {
            otSerialSignalReceive();
            sem_wait(s_semaphore);
        }
    }

    return NULL;
}

const uint8_t *otSerialGetReceivedBytes(uint16_t *aBufLength)
{
    size_t length;
    int i;

    length = read(s_in_fd, s_receive_buffer, sizeof(s_receive_buffer));

    for (i = 0; i < length; i++)
    {
        if (s_receive_buffer[i] == 0x03)
        {
            otSerialDisable();
            exit(0);
        }
    }

    if (aBufLength != NULL)
    {
        *aBufLength = length;
    }

    return s_receive_buffer;
}

void otSerialHandleReceiveDone(void)
{
    sem_post(s_semaphore);
}
