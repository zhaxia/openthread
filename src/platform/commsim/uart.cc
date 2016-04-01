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

#include <platform/common/uart.h>
#include <platform/posix/cmdline.h>
#include <common/code_utils.h>
#include <common/tasklet.h>
#include <pthread.h>
#include <semaphore.h>
#include <termios.h>
#include <unistd.h>
// Linux build needs these
#include <stdlib.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

// COMMSIM DIFF extern struct gengetopt_args_info args_info;
extern void uart_handle_receive(uint8_t *buf, uint16_t buf_length);
extern void uart_handle_send_done();

static void uart_receive_task(void *context);
static void uart_send_task(void *context);
static void *uart_receive_thread(void *arg);

static Thread::Tasklet s_receive_task(&uart_receive_task, NULL);
static Thread::Tasklet s_send_task(&uart_send_task, NULL);
static int s_fd;
static pthread_t s_pthread;
static sem_t *s_semaphore;

ThreadError uart_start()
{
    ThreadError error = kThreadError_None;
    struct termios termios;
    char *path;
    char cmd[256];

    // open file
#if __APPLE__

// COMMSIM DIFF    asprintf(&path, "/dev/ptyp%d", args_info.eui64_arg);
    VerifyOrExit((s_fd = open(path, O_RDWR | O_NOCTTY)) >= 0, perror("posix_openpt"); error = kThreadError_Error);
    free(path);

    // print pty path
// COMMSIM DIFF    printf("/dev/ttyp%d\n", args_info.eui64_arg);

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

// COMMSIM DIFF    snprintf(cmd, sizeof(cmd), "thread_uart_semaphore_%d", args_info.eui64_arg);
    s_semaphore = sem_open(cmd, O_CREAT, 0644, 0);
    pthread_create(&s_pthread, NULL, &uart_receive_thread, NULL);

    return error;

exit:
    close(s_fd);
    return error;
}

ThreadError uart_stop()
{
    ThreadError error = kThreadError_None;

    close(s_fd);

    return error;
}

ThreadError uart_send(const uint8_t *buf, uint16_t buf_length)
{
    ThreadError error = kThreadError_None;

    VerifyOrExit(write(s_fd, buf, buf_length) >= 0, error = kThreadError_Error);
    s_send_task.Post();

exit:
    return error;
}

void uart_send_task(void *context)
{
    uart_handle_send_done();
}

void *uart_receive_thread(void *arg)
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
            s_receive_task.Post();
            sem_wait(s_semaphore);
        }
    }

    return NULL;
}

void uart_receive_task(void *context)
{
    uint8_t receive_buffer[1024];
    size_t len;

    len = read(s_fd, receive_buffer, sizeof(receive_buffer));
    uart_handle_receive(receive_buffer, len);

    sem_post(s_semaphore);
}

#ifdef __cplusplus
}  // end of extern "C"
#endif
