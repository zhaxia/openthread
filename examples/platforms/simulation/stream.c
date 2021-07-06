
#include "platform-simulation.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <openthread/platform/stream.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"

#define BASE_PORT 8887
#ifndef MS_PER_S
#define MS_PER_S 1000
#endif
#ifndef US_PER_MS
#define US_PER_MS 1000
#endif
#ifndef US_PER_S
#define US_PER_S (MS_PER_S * US_PER_MS)
#endif
#ifndef NS_PER_US
#define NS_PER_US 1000
#endif

static int                sSockFd = -1;
static uint16_t           sLocalPort;
static uint16_t           sPeerPort;
static struct sockaddr_in sPeerAddr;

// static const uint8_t *sWriteBuffer = NULL;
static uint16_t sWriteLength = 0;

static void PortInit(uint8_t aId)
{
    if (aId % 2 == 1)
    {
        sLocalPort = BASE_PORT + 1;
        sPeerPort  = BASE_PORT + 2;
    }
    else
    {
        sLocalPort = BASE_PORT + 2;
        sPeerPort  = BASE_PORT + 1;
    }
}

otError otPlatStreamEnable(uint8_t aId)
{
    otError            error = OT_ERROR_NONE;
    struct sockaddr_in addr;

    PortInit(aId);

    VerifyOrExit((sSockFd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, error = OT_ERROR_FAILED);

    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(sLocalPort);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(addr.sin_zero, 0, 8);

    int re_flag = 1;
    int re_len  = sizeof(int);

    setsockopt(sSockFd, SOL_SOCKET, SO_REUSEADDR, &re_flag, re_len);
    VerifyOrExit(bind(sSockFd, (const struct sockaddr *)&addr, sizeof(struct sockaddr)) >= 0, error = OT_ERROR_FAILED);

    sPeerAddr.sin_family      = AF_INET;
    sPeerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sPeerAddr.sin_port        = htons(sPeerPort);

    // otLogCritMac("otPlatStreamEnable: LocalPort:%d, PeerPort:%d", sLocalPort, sPeerPort);

exit:
    return error;
}

otError otPlatStreamDisable(void)
{
    if (sSockFd >= 0)
    {
        close(sSockFd);
        sSockFd      = -1;
        sWriteLength = 0;
    }

    return OT_ERROR_NONE;
}

otError otPlatStreamSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    otError error = OT_ERROR_NONE;
    int     rval;

    VerifyOrExit(sSockFd >= 0, error = OT_ERROR_INVALID_STATE);
    // VerifyOrExit(sWriteLength == 0, error = OT_ERROR_BUSY);

    // sWriteBuffer = aBuf;
    // sWriteLength = aBufLength;

    // otLogCritMac("otPlatStreamSend");

    while (aBufLength > 0)
    {
        rval = sendto(sSockFd, aBuf, aBufLength, 0, (struct sockaddr *)&sPeerAddr, sizeof(sPeerAddr));

        aBuf += rval;
        aBufLength -= (uint16_t)rval;

        if (aBufLength == 0)
        {
            otPlatStreamSendDone();
        }
    }

exit:
    return error;
}

void platformStreamUpdateFdSet(fd_set *aReadFdSet, fd_set *aWriteFdSet, fd_set *aErrorFdSet, int *aMaxFd)
{
    int fd = sSockFd;

    VerifyOrExit(fd >= 0);

    if (aReadFdSet != NULL)
    {
        FD_SET(fd, aReadFdSet);

        if (aErrorFdSet != NULL)
        {
            FD_SET(fd, aErrorFdSet);
        }

        if (aMaxFd != NULL && *aMaxFd < fd)
        {
            *aMaxFd = fd;
        }
    }

    (void)aWriteFdSet;
#if 0
    if ((aWriteFdSet != NULL) && (sWriteLength > 0))
    {
        // otLogCritMac("platformStreamUpdateFdSet: aWriteFdSet");
        FD_SET(fd, aWriteFdSet);

        if (aErrorFdSet != NULL)
        {
            FD_SET(fd, aErrorFdSet);
        }

        if (aMaxFd != NULL && *aMaxFd < fd)
        {
            *aMaxFd = fd;
        }
    }
#endif

exit:
    return;
}

otError otPlatStreamBlockingRead(uint8_t *aBuf, uint16_t *aBufLength, uint64_t aTimeoutUs)
{
    otError        error = OT_ERROR_NONE;
    struct timeval timeout;
    int            rval;
    fd_set         read_fds;
    fd_set         error_fds;

    timeout.tv_sec  = (time_t)(aTimeoutUs / US_PER_S);
    timeout.tv_usec = (suseconds_t)(aTimeoutUs % US_PER_S);

    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    FD_SET(sSockFd, &read_fds);
    FD_SET(sSockFd, &error_fds);

    rval = select(sSockFd + 1, &read_fds, NULL, &error_fds, &timeout);

    if (rval > 0)
    {
        if (FD_ISSET(sSockFd, &read_fds))
        {
            rval = read(sSockFd, aBuf, *aBufLength);
            if (rval < 0)
            {
                otLogCritPlat("Stream Error");
                exit(-1);
            }
            else
            {
                *aBufLength = (uint16_t)rval;
            }
        }
        else if (FD_ISSET(sSockFd, &error_fds))
        {
            otLogCritPlat("Stream Error");
            exit(-1);
        }
        else
        {
            otLogCritPlat("Stream Error");
            exit(-1);
        }
    }
    else if (rval == 0)
    {
        ExitNow(error = OT_ERROR_RESPONSE_TIMEOUT);
    }
    else if (errno != EINTR)
    {
        otLogCritPlat("Stream Error");
        exit(-1);
    }

exit:
    return error;
}

void platformStreamProcess(otInstance *  aInstance,
                           const fd_set *aReadFdSet,
                           const fd_set *aWriteFdSet,
                           const fd_set *aErrorFdSet)
{
    int fd = sSockFd;

    (void)aInstance;
    VerifyOrExit(fd >= 0);

    if (FD_ISSET(fd, aErrorFdSet))
    {
        otLogCritPlat("Stream Error");
        exit(-1);
    }

    if (FD_ISSET(fd, aReadFdSet))
    {
        ssize_t rval;
        uint8_t buffer[256];

        // otLogCritMac("platformStreamProcess: read");
        rval = read(fd, buffer, sizeof(buffer));

        if (rval > 0)
        {
            otPlatStreamReceived(buffer, (uint16_t)rval);
        }
        else if (rval <= 0)
        {
            if (rval < 0)
            {
                perror("Stream read");
            }

            close(sSockFd);
            sSockFd = -1;
            ExitNow();
        }
    }

    (void)aWriteFdSet;
#if 0
    if ((FD_ISSET(fd, aWriteFdSet)))
    {
        ssize_t rval;

        // otLogCritMac("platformStreamProcess: sendto");
        // otLogCritMac("platformStreamProcess: NodeId:%d, LocalPort:%d, PeerPort:%d", gNodeId, sLocalPort, sPeerPort);
        rval = sendto(fd, sWriteBuffer, sWriteLength, 0, (struct sockaddr *)&sPeerAddr, sizeof(sPeerAddr));
        VerifyOrExit(rval >= 0, otLogWarnPlat("Stream send failed"));

        sWriteBuffer += rval;
        sWriteLength -= (uint16_t)rval;

        if (sWriteLength == 0)
        {
            otPlatStreamSendDone();
        }
    }
#endif
exit:
    return;
}

otError otPlatStreamFlush(void)
{
    return OT_ERROR_NONE;
}

OT_TOOL_WEAK void otPlatStreamSendDone(void)
{
}

OT_TOOL_WEAK void otPlatStreamReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    (void)aBuf;
    (void)aBufLength;
}
