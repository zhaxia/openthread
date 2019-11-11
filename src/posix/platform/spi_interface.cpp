/*
 *  Copyright (c) 2019, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes the implementation for the SPI interface to radio (RCP).
 */

#include "openthread-core-config.h"
#include "platform-posix.h"

#include "spi_interface.hpp"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/ucontext.h>

#include <linux/ioctl.h>
#include <linux/spi/spidev.h>

#if OPENTHREAD_POSIX_NCP_SPI_ENABLE

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC 1000
#endif

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC 1000
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC (USEC_PER_MSEC * MSEC_PER_SEC)
#endif

#define GPIO_INT_ASSERT_STATE 0 // I̅N̅T̅ is asserted low
#define GPIO_RES_ASSERT_STATE 0 // R̅E̅S̅ is asserted low

namespace ot {
namespace PosixApp {

SpiInterface::SpiInterface(Callbacks &aCallbacks)
    : SpinelInterface()
    , mIsDecoding(false)
    , mCallbacks(aCallbacks)
    , sSpiDevFd(-1)
    , sResGpioValueFd(-1)
    , sIntGpioValueFd(-1)
    , sSpiSpeed(kSpiSpeed)
    , sSpiMode(kSpiMode)
    , sSpiCsDelay(kSpiCsDelay)
    , sSpiResetDelay(kSpiResetDelay)
    , sSlaveResetCount(0)
    , sSpiFrameCount(0)
    , sSpiValidFrameCount(0)
    , sSpiGarbageFrameCount(0)
    , sSpiDuplexFrameCount(0)
    , sSpiUnresponsiveFrameCount(0)
    , sSpiRxFrameCount(0)
    , sSpiRxFrameByteCount(0)
    , sSpiTxFrameCount(0)
    , sSpiTxFrameByteCount(0)
    , sSpiRxPayloadSize(0)
    , sSpiTxIsReady(false)
    , sSpiTxRefusedCount(0)
    , sSpiTxPayloadSize(0)
    , sSpiRxAlignAllowance(0)
    , sSpiSmallPacketSize(32)
    , sSlaveDidReset(false)
    , sLogLevel(LOG_WARNING)
    , did_print_rate_limit_log(false)
    , mWritePointer(GetRxFrameBuffer())
{

}

otError SpiInterface::Init(otPlatformConfig *aConfig)
{
    ResetGpioInit(aConfig->mResetPinPath);
    IntGpioInit(aConfig->mIntPinPath);
    SpiDevInit(aConfig->mRadioFile, aConfig->mMode, aConfig->mSpeed);

    Trigerreset();
    usleep((useconds_t)aConfig->mResetDelay * USEC_PER_MSEC);

    return OT_ERROR_NONE;
}

void SpiInterface::Deinit(void)
{
    if (sSpiDevFd >= 0)
    {
        close(sSpiDevFd);
    }

    if (sResGpioValueFd >= 0)
    {
        close(sResGpioValueFd);
    }

    if (sIntGpioValueFd >= 0)
    {
        close(sIntGpioValueFd);
    }

    // This allows initializing all variables.
    // new (this) SpiInterface(mCallbacks);
}

void SpiInterface::ResetGpioInit(const char *aPath)
{
    int   setup_fd   = -1;
    char *dir_path   = NULL;
    char *value_path = NULL;

    VerifyOrExit(aPath != NULL);

    otLogDebgPlat("[SPI] Reset GPIO path: %s", aPath);

    VerifyOrExit(asprintf(&dir_path, "%s/direction", aPath) > 0, perror("asprintf"));
    VerifyOrExit(asprintf(&value_path, "%s/value", aPath) > 0, perror("asprintf"));

    VerifyOrExit(setup_fd = open(dir_path, O_WRONLY | O_CLOEXEC) > 0, perror("open_reset_dir_path"));
    VerifyOrExit(write(setup_fd, "high\n", 5) > 0, perror("set_reset_direction"));

    VerifyOrDie((sResGpioValueFd = open(value_path, O_WRONLY | O_CLOEXEC)) >= 0, OT_EXIT_FAILURE);

exit:
    if (setup_fd >= 0)
    {
        close(setup_fd);
    }

    if (dir_path)
    {
        free(dir_path);
    }

    if (value_path)
    {
        free(value_path);
    }
}

void SpiInterface::Trigerreset(void)
{
    char str[] = {'0' + GPIO_RES_ASSERT_STATE, '\n'};

    lseek(sResGpioValueFd, 0, SEEK_SET);
    if (write(sResGpioValueFd, str, sizeof(str)) == -1)
    {
        otLogWarnPlat("[SPI] Trigerreset(): error on write: %d (%s)", errno, strerror(errno));
    }

    usleep(10 * USEC_PER_MSEC);

    // Set the string to switch to the not-asserted state.
    str[0] = '0' + !GPIO_RES_ASSERT_STATE;

    lseek(sResGpioValueFd, 0, SEEK_SET);
    if (write(sResGpioValueFd, str, sizeof(str)) == -1)
    {
        otLogWarnPlat("[SPI]Trigerreset(): error on write: %d (%s)", errno, strerror(errno));
    }

    otLogNotePlat("[SPI] Triggered hardware reset");
}

void SpiInterface::IntGpioInit(const char *path)
{
    char *edge_path  = NULL;
    char *dir_path   = NULL;
    char *value_path = NULL;
    int   setup_fd   = -1;

    VerifyOrExit(path != NULL);

    otLogDebgPlat("[SPI] Reset GPIO path: %s", aPath);

    VerifyOrExit(asprintf(&dir_path, "%s/direction", path) > 0, perror("asprintf"));
    VerifyOrExit(asprintf(&edge_path, "%s/edge", path) > 0, perror("asprintf"));
    VerifyOrExit(asprintf(&value_path, "%s/value", path) > 0, perror("asprintf"));

    VerifyOrExit((setup_fd = open(dir_path, O_WRONLY | O_CLOEXEC)) >= 0, perror("open_int_dir_path"));
    VerifyOrExit(write(setup_fd, "in", 2) > 0, perror("write_int_dir_path"));
    close(setup_fd);

    VerifyOrExit((setup_fd = open(edge_path, O_WRONLY | O_CLOEXEC)) >= 0, perror("open_int_edge_path"));
    VerifyOrExit(write(setup_fd, "falling", 7) > 0, perror("write_int_edge_path"));
    close(setup_fd);

    setup_fd = -1;

    VerifyOrDie((sIntGpioValueFd = open(value_path, O_RDONLY | O_CLOEXEC)) >= 0, OT_EXIT_FAILURE);

exit:

    if (setup_fd >= 0)
    {
        close(setup_fd);
    }

    if (edge_path)
    {
        free(edge_path);
    }

    if (dir_path)
    {
        free(dir_path);
    }

    if (value_path)
    {
        free(value_path);
    }
}

void SpiInterface::SpiDevInit(const char *aPath, uint8_t aMode, uint32_t aSpeed)
{
    int           fd       = -1;
    const uint8_t wordBits = 8;

    VerifyOrDie(aPath != NULL, OT_EXIT_FAILURE);

    otLogDebgPlat("[SPI] SPI device path: %s", aPath);

    VerifyOrExit((fd = open(aPath, O_RDWR | O_CLOEXEC)) >= 0, perror("open_spi_dev_path"));
    VerifyOrExit(ioctl(fd, SPI_IOC_WR_MODE, &aMode) >= 0, perror("ioctl(SPI_IOC_WR_MODE)"));
    VerifyOrExit(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &aSpeed) >= 0, perror("ioctl(SPI_IOC_WR_MAX_SPEED_HZ)"));
    VerifyOrExit(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &wordBits) >= 0, perror("ioctl(SPI_IOC_WR_BITS_PER_WORD)"));
    VerifyOrExit(flock(fd, LOCK_EX | LOCK_NB) >= 0, perror("flock"));

    sSpiDevFd   = fd;
    sSpiMode    = aMode;
    sSpiSpeed   = aSpeed;
    fd          = -1;

exit:
    if (fd >= 0)
    {
        close(fd);
    }

    VerifyOrDie(sSpiDevFd >= 0, OT_EXIT_FAILURE);
}

void SpiInterface::log_debug_buffer(const char *desc, const uint8_t *buffer_ptr, int buffer_len, bool force)
{
    int i = 0;

    while (i < buffer_len)
    {
        int  j;
        char dump_string[SOCKET_DEBUG_BYTES_PER_LINE * 3 + 1];

        for (j = 0; i < buffer_len && j < SOCKET_DEBUG_BYTES_PER_LINE; i++, j++)
        {
            sprintf(dump_string + j * 3, "%02X ", buffer_ptr[i]);
        }

        if (force)
        {
            otLogWarnPlat("[SPI] %s: %s%s", desc, dump_string, (i < buffer_len) ? " ..." : "");
        }
        else
        {
            otLogDebgPlat("[SPI] %s: %s%s", desc, dump_string, (i < buffer_len) ? " ..." : "");
        }
    }

    OT_UNUSED_VARIABLE(desc);
}

void SpiInterface::spi_header_set_flag_byte(uint8_t *header, uint8_t value)
{
    header[0] = value;
}

void SpiInterface::spi_header_set_accept_len(uint8_t *header, uint16_t len)
{
    header[1] = ((len >> 0) & 0xFF);
    header[2] = ((len >> 8) & 0xFF);
}

void SpiInterface::spi_header_set_data_len(uint8_t *header, uint16_t len)
{
    header[3] = ((len >> 0) & 0xFF);
    header[4] = ((len >> 8) & 0xFF);
}

uint8_t SpiInterface::spi_header_get_flag_byte(const uint8_t *header)
{
    return header[0];
}

uint16_t SpiInterface::spi_header_get_accept_len(const uint8_t *header)
{
    return (header[1] + (uint16_t)(header[2] << 8));
}

uint16_t SpiInterface::spi_header_get_data_len(const uint8_t *header)
{
    return (header[3] + (uint16_t)(header[4] << 8));
}

uint8_t *SpiInterface::get_real_rx_frame_start(void)
{
    uint8_t *ret = sSpiRxFrameBuffer;
    int      i   = 0;

    for (i = 0; i < sSpiRxAlignAllowance; i++)
    {
        if (ret[0] != 0xFF)
        {
            break;
        }
        ret++;
    }

    return ret;
}

int SpiInterface::do_spi_xfer(int len)
{
    int ret;

    struct spi_ioc_transfer xfer[2] = {{
                                           // This part is the delay between C̅S̅ being asserted and the SPI clock
                                           // starting. This is not supported by all Linux SPI drivers.
                                           .tx_buf        = 0,
                                           .rx_buf        = 0,
                                           .len           = 0,
                                           .speed_hz      = (uint32_t)sSpiSpeed,
                                           .delay_usecs   = (uint16_t)sSpiCsDelay,
                                           .bits_per_word = 8,
                                           .cs_change     = false,
                                           .tx_nbits      = 0,
                                           .rx_nbits      = 0,
                                           .word_delay_usecs = 0,
                                           .pad           = 0,
                                       },
                                       {
                                           // This part is the actual SPI transfer.
                                           .tx_buf        = (unsigned long)sSpiTxFrameBuffer,
                                           .rx_buf        = (unsigned long)sSpiRxFrameBuffer,
                                           .len           = (uint32_t)(len + HEADER_LEN + sSpiRxAlignAllowance),
                                           .speed_hz      = (uint32_t)sSpiSpeed,
                                           .delay_usecs   = 0,
                                           .bits_per_word = 8,
                                           .cs_change     = false,
                                           .tx_nbits      = 0,
                                           .rx_nbits      = 0,
                                           .word_delay_usecs = 0,
                                           .pad           = 0,
                                       }};

    if (sSpiCsDelay > 0)
    {
        // A C̅S̅ delay has been specified. Start transactions with both parts.
        ret = ioctl(sSpiDevFd, SPI_IOC_MESSAGE(2), &xfer[0]);
    }
    else
    {
        // No C̅S̅ delay has been specified, so we skip the first part because it causes some SPI drivers to croak.
        ret = ioctl(sSpiDevFd, SPI_IOC_MESSAGE(1), &xfer[1]);
    }

    if (ret != -1)
    {
        log_debug_buffer("SPI-TX", sSpiTxFrameBuffer, (int)xfer[1].len, false);
        log_debug_buffer("SPI-RX", sSpiRxFrameBuffer, (int)xfer[1].len, false);

        sSpiFrameCount++;
    }

    return ret;
}

int SpiInterface::push_pull_spi(void)
{
    int            ret;
    uint16_t       spi_xfer_bytes   = 0;
    const uint8_t *spiRxFrameBuffer = NULL;
    uint8_t        slave_header;
    uint16_t       slave_accept_len;
    int            successful_exchanges = 0;

    static uint16_t slave_data_len;

    // For now, sSpiRxPayloadSize must be zero when entering this function. This may change at some point, for now
    // this makes things much easier.
    assert(sSpiRxPayloadSize == 0);

    if (sSpiValidFrameCount == 0)
    {
        // Set the reset flag to indicate to our slave that we are coming up from scratch.
        spi_header_set_flag_byte(sSpiTxFrameBuffer, SPI_HEADER_RESET_FLAG | SPI_HEADER_PATTERN_VALUE);
    }
    else
    {
        spi_header_set_flag_byte(sSpiTxFrameBuffer, SPI_HEADER_PATTERN_VALUE);
    }

    // Zero out our rx_accept and our data_len for now.
    spi_header_set_accept_len(sSpiTxFrameBuffer, 0);
    spi_header_set_data_len(sSpiTxFrameBuffer, 0);

    // Sanity check.
    if (slave_data_len > MAX_FRAME_SIZE)
    {
        slave_data_len = 0;
    }

    if (sSpiTxIsReady)
    {
        // Go ahead and try to immediately send a frame if we have it queued up.
        spi_header_set_data_len(sSpiTxFrameBuffer, sSpiTxPayloadSize);

        if (sSpiTxPayloadSize > spi_xfer_bytes)
        {
            spi_xfer_bytes = sSpiTxPayloadSize;
        }
    }

    if (sSpiRxPayloadSize == 0)
    {
        if (slave_data_len != 0)
        {
            // In a previous transaction the slave indicated it had something to send us. Make sure our transaction
            // is large enough to handle it.
            if (slave_data_len > spi_xfer_bytes)
            {
                spi_xfer_bytes = slave_data_len;
            }
        }
        else
        {
            // Set up a minimum transfer size to allow small frames the slave wants to send us to be handled in a
            // single transaction.
            if (sSpiSmallPacketSize > spi_xfer_bytes)
            {
                spi_xfer_bytes = (uint16_t)sSpiSmallPacketSize;
            }
        }

        spi_header_set_accept_len(sSpiTxFrameBuffer, spi_xfer_bytes);
    }

    // Perform the SPI transaction.
    ret = do_spi_xfer(spi_xfer_bytes);

    if (ret < 0)
    {
        perror("push_pull_spi:do_spi_xfer");
        otLogWarnPlat("[SPI] push_pull_spi:do_spi_xfer: errno=%d (%s)", errno, strerror(errno));

        // Print out a helpful error message for a common error.
        if ((sSpiCsDelay != 0) && (errno == EINVAL))
        {
            otLogWarnPlat("[SPI] SPI ioctl failed with EINVAL. Try adding `--spi-cs-delay=0` to command line arguments.");
        }

        ExitNow();
    }

    // Account for misalignment (0xFF bytes at the start)
    spiRxFrameBuffer = get_real_rx_frame_start();

    otLogDebgPlat("[SPI] spi_xfer TX: H:%02X ACCEPT:%d DATA:%0d", spi_header_get_flag_byte(sSpiTxFrameBuffer),
                  spi_header_get_accept_len(sSpiTxFrameBuffer), spi_header_get_data_len(sSpiTxFrameBuffer));
    otLogDebgPlat("[SPI] spi_xfer RX: H:%02X ACCEPT:%d DATA:%0d", spi_header_get_flag_byte(spiRxFrameBuffer),
                  spi_header_get_accept_len(spiRxFrameBuffer), spi_header_get_data_len(spiRxFrameBuffer));

    slave_header = spi_header_get_flag_byte(spiRxFrameBuffer);

    if ((slave_header == 0xFF) || (slave_header == 0x00))
    {
        if ((slave_header == spiRxFrameBuffer[1]) && (slave_header == spiRxFrameBuffer[2]) &&
            (slave_header == spiRxFrameBuffer[3]) && (slave_header == spiRxFrameBuffer[4]))
        {
            // Device is off or in a bad state. In some cases may be induced by flow control.
            if (slave_data_len == 0)
            {
                otLogDebgPlat("[SPI] Slave did not respond to frame. (Header was all 0x%02X)", slave_header);
            }
            else
            {
                otLogWarnPlat("[SPI] Slave did not respond to frame. (Header was all 0x%02X)", slave_header);
            }

            sSpiUnresponsiveFrameCount++;
        }
        else
        {
            // Header is full of garbage
            otLogWarnPlat("[SPI] Garbage in header : %02X %02X %02X %02X %02X", spiRxFrameBuffer[0],
                          spiRxFrameBuffer[1], spiRxFrameBuffer[2], spiRxFrameBuffer[3], spiRxFrameBuffer[4]);
            sSpiGarbageFrameCount++;

#if OPENTHREAD_CONFIG_LOG_LEVEL < OT_LOG_LEVEL_DEBG
            log_debug_buffer("SPI-TX", sSpiTxFrameBuffer, (int)spi_xfer_bytes + HEADER_LEN + sSpiRxAlignAllowance,
                             true);
            log_debug_buffer("SPI-RX", sSpiRxFrameBuffer, (int)spi_xfer_bytes + HEADER_LEN + sSpiRxAlignAllowance,
                             true);
#endif
        }

        sSpiTxRefusedCount++;
        ExitNow();
    }

    slave_accept_len = spi_header_get_accept_len(spiRxFrameBuffer);
    slave_data_len   = spi_header_get_data_len(spiRxFrameBuffer);

    if (((slave_header & SPI_HEADER_PATTERN_MASK) != SPI_HEADER_PATTERN_VALUE) || (slave_accept_len > MAX_FRAME_SIZE) ||
        (slave_data_len > MAX_FRAME_SIZE))
    {
        sSpiGarbageFrameCount++;
        sSpiTxRefusedCount++;
        slave_data_len = 0;
        otLogWarnPlat("[SPI] Garbage in header : %02X %02X %02X %02X %02X", spiRxFrameBuffer[0], spiRxFrameBuffer[1],
               spiRxFrameBuffer[2], spiRxFrameBuffer[3], spiRxFrameBuffer[4]);
#if OPENTHREAD_CONFIG_LOG_LEVEL < OT_LOG_LEVEL_DEBG
        log_debug_buffer("SPI-TX", sSpiTxFrameBuffer, (int)spi_xfer_bytes + HEADER_LEN + sSpiRxAlignAllowance,
                         true);
        log_debug_buffer("SPI-RX", sSpiRxFrameBuffer, (int)spi_xfer_bytes + HEADER_LEN + sSpiRxAlignAllowance,
                         true);
#endif
        ExitNow();
    }

    sSpiValidFrameCount++;

    if ((slave_header & SPI_HEADER_RESET_FLAG) == SPI_HEADER_RESET_FLAG)
    {
        sSlaveResetCount++;
        otLogNotePlat("[SPI] Slave did reset (%llu resets so far)", (unsigned long long)sSlaveResetCount);
        sSlaveDidReset = true;
        LogStats();
    }

    // Handle received packet, if any.
    if ((sSpiRxPayloadSize == 0) && (slave_data_len != 0) && (slave_data_len <= slave_accept_len))
    {
        // We have received a packet. Set sSpiRxPayloadSize so that the packet will eventually get queued up by
        // push_hdlc().
        sSpiRxPayloadSize = slave_data_len;
        slave_data_len    = 0;

        successful_exchanges++;

        sSpiRxFrameCount++;
        sSpiRxFrameByteCount += sSpiRxPayloadSize;
    }

    // Handle transmitted packet, if any.
    if (sSpiTxIsReady && (sSpiTxPayloadSize == spi_header_get_data_len(sSpiTxFrameBuffer)))
    {
        if (spi_header_get_data_len(sSpiTxFrameBuffer) <= slave_accept_len)
        {
            // Our outbound packet has been successfully transmitted. Clear sSpiTxPayloadSize and sSpiTxIsReady so
            // that pull_hdlc() can pull another packet for us to send.
            successful_exchanges++;

            sSpiTxFrameCount++;
            sSpiTxFrameByteCount += sSpiTxPayloadSize;

            sSpiTxIsReady      = false;
            sSpiTxPayloadSize  = 0;
            sSpiTxRefusedCount = 0;
        }
        else
        {
            // The slave Wasn't ready for what we had to send them. Incrementing this counter will turn on rate
            // limiting so that we don't waste a ton of CPU bombarding them with useless SPI transfers.
            sSpiTxRefusedCount++;
        }
    }

    if (!sSpiTxIsReady)
    {
        sSpiTxRefusedCount = 0;
    }

    if (successful_exchanges == 2)
    {
        sSpiDuplexFrameCount++;
    }

exit:
    return ret;
}

bool SpiInterface::check_and_clear_interrupt(void)
{
    bool ret = true;

    if (sIntGpioValueFd >= 0)
    {
        char value[5] = "";

        lseek(sIntGpioValueFd, 0, SEEK_SET);

        VerifyOrDie(read(sIntGpioValueFd, value, sizeof(value) - 1) > 0, OT_EXIT_FAILURE);

        // The interrupt pin is active low.
        ret = (GPIO_INT_ASSERT_STATE == atoi(value));
    }

    return ret;
}

void SpiInterface::UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd,
                               struct timeval &aTimeout)
{
    int timeout_ms = MSEC_PER_SEC * 60 * 60 * 24; // 24 hours

    OT_UNUSED_VARIABLE(aReadFdSet);
    OT_UNUSED_VARIABLE(aWriteFdSet);

    if (aMaxFd < sIntGpioValueFd)
    {
        aMaxFd = sIntGpioValueFd;
    }

    if (sSpiTxIsReady)
    {
        // We have data to send to the slave.
        timeout_ms = 0;
    }

    if (sIntGpioValueFd >= 0)
    {
        if (check_and_clear_interrupt())
        {
            // Interrupt pin is asserted, set the timeout to be 0.
            timeout_ms = 0;
            otLogDebgPlat("[SPI] Interrupt.");
        }
        else
        {
            // The interrupt pin was not asserted, so we wait for the interrupt pin to be asserted by adding it to the
            // error set.
            FD_SET(sIntGpioValueFd, &aErrorFdSet);
        }
    }
    else if (timeout_ms > SPI_POLL_PERIOD_MSEC)
    {
        // In this case we don't have an interrupt, so we revert to SPI polling.
        timeout_ms = SPI_POLL_PERIOD_MSEC;
    }

    if (sSpiTxRefusedCount)
    {
        int min_timeout = 0;

        // We are being rate-limited by the slave. This is fairly normal behavior. Based on number of times slave has
        // refused a transmission, we apply a minimum timeout.

        if (sSpiTxRefusedCount < IMMEDIATE_RETRY_COUNT)
        {
            min_timeout = IMMEDIATE_RETRY_TIMEOUT_MSEC;
        }
        else if (sSpiTxRefusedCount < FAST_RETRY_COUNT)
        {
            min_timeout = FAST_RETRY_TIMEOUT_MSEC;
        }
        else
        {
            min_timeout = SLOW_RETRY_TIMEOUT_MSEC;
        }

        if (timeout_ms < min_timeout)
        {
            timeout_ms = min_timeout;
        }

        if (sSpiTxIsReady && !did_print_rate_limit_log && (sSpiTxRefusedCount > 1))
        {
            // To avoid printing out this message over and over, we only print it out once the refused count is at two
            // or higher when we actually have something to send the slave. And then, we only print it once.
            otLogInfoPlat("[SPI] Slave is rate limiting transactions");

            did_print_rate_limit_log = true;
        }

        if (sSpiTxRefusedCount == 30)
        {
            // Ua-oh. The slave hasn't given us a chance to send it anything for over thirty frames. If this ever
            // happens, print out a warning to the logs.
            otLogNotePlat("[SPI] Slave seems stuck.");
        }

        if (sSpiTxRefusedCount == 100)
        {
            // Double ua-oh. The slave hasn't given us a chance to send it anything for over a hundred frames.
            // This almost certainly means that the slave has locked up or gotten into an unrecoverable state.
            // It is not spi-hdlc-adapter's job to identify and reset misbehaving devices (that is handled at a
            // higher level), but we go ahead and log the condition for debugging purposes.
            otLogCritPlat("[SPI] Slave seems REALLY stuck.");

           // TODO: process transmitting failure.
        }
    }
    else
    {
        did_print_rate_limit_log = false;
    }

    if (static_cast<uint64_t>(timeout_ms * US_PER_MS) < static_cast<uint64_t>(aTimeout.tv_sec * US_PER_S + aTimeout.tv_usec))
    {
        aTimeout.tv_sec  = static_cast<time_t>(timeout_ms / MSEC_PER_SEC);
        aTimeout.tv_usec = static_cast<suseconds_t>((timeout_ms % MSEC_PER_SEC) * USEC_PER_MSEC);
    }
}

void SpiInterface::ClearStats(void)
{
    sSlaveResetCount           = 0;
    sSpiFrameCount             = 0;
    sSpiValidFrameCount        = 0;
    sSpiGarbageFrameCount      = 0;
    sSpiDuplexFrameCount       = 0;
    sSpiUnresponsiveFrameCount = 0;
    sSpiRxFrameCount           = 0;
    sSpiRxFrameByteCount       = 0;
    sSpiTxFrameCount           = 0;
    sSpiTxFrameByteCount       = 0;
}

void SpiInterface::LogStats(void)
{
    otLogInfoPlat("INFO: sSlaveResetCount=%llu", (unsigned long long)sSlaveResetCount);
    otLogInfoPlat("INFO: sSpiFrameCount=%llu", (unsigned long long)sSpiFrameCount);
    otLogInfoPlat("INFO: sSpiValidFrameCount=%llu", (unsigned long long)sSpiValidFrameCount);
    otLogInfoPlat("INFO: sSpiDuplexFrameCount=%llu", (unsigned long long)sSpiDuplexFrameCount);
    otLogInfoPlat("INFO: sSpiUnresponsiveFrameCount=%llu", (unsigned long long)sSpiUnresponsiveFrameCount);
    otLogInfoPlat("INFO: sSpiGarbageFrameCount=%llu", (unsigned long long)sSpiGarbageFrameCount);
    otLogInfoPlat("INFO: sSpiRxFrameCount=%llu", (unsigned long long)sSpiRxFrameCount);
    otLogInfoPlat("INFO: sSpiRxFrameByteCount=%llu", (unsigned long long)sSpiRxFrameByteCount);
    otLogInfoPlat("INFO: sSpiTxFrameCount=%llu", (unsigned long long)sSpiTxFrameCount);
    otLogInfoPlat("INFO: sSpiTxFrameByteCount=%llu", (unsigned long long)sSpiTxFrameByteCount);
}

void SpiInterface::Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet)
{
    OT_UNUSED_VARIABLE(aReadFdSet);
    OT_UNUSED_VARIABLE(aWriteFdSet);

    // Service the SPI port if we can receive a packet or we have a packet to be sent.
    if ((sSpiRxPayloadSize == 0) && (sSpiTxIsReady || check_and_clear_interrupt()))
    {
        // We guard this with the above check because we don't want to overwrite any previously received (but not yet
        // pushed out) frames.
        if (push_pull_spi() < 0)
        {
            LogStats();
            DieNow(EXIT_FAILURE);
        }
    }

    // Process the received packet.
    if (sSpiRxPayloadSize != 0)
    {
        const uint8_t * spiRxFrameBuffer = get_real_rx_frame_start();
        uint16_t        i;
        uint8_t         byte;

        for (i = 0; i < sSpiRxPayloadSize; i++)
        {
            byte = spiRxFrameBuffer[i + HEADER_LEN];
            if (mWritePointer.CanWrite(sizeof(uint8_t)))
            {
                mWritePointer.WriteByte(byte);
            }
            else
            {
                break;
            }
        }

        if (i >= sSpiRxPayloadSize)
        {
            mIsDecoding = true;
            mCallbacks.HandleReceivedFrame(*this);
            mIsDecoding = false;
        }

        sSpiRxPayloadSize = 0;
    }
}

otError SpiInterface::WaitResponse(struct timeval &aTimeout)
{
    otError error = OT_ERROR_NONE;
    fd_set  read_fds;
    fd_set  error_fds;
    int rval;

    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    FD_SET(sIntGpioValueFd, &read_fds);
    FD_SET(sIntGpioValueFd, &error_fds);

    rval = select(sIntGpioValueFd + 1, &read_fds, NULL, &error_fds, &aTimeout);

    if (rval > 0)
    {
        if (FD_ISSET(sIntGpioValueFd, &read_fds))
        {
            // Read();
            push_pull_spi();
        }
        else if (FD_ISSET(sIntGpioValueFd, &error_fds))
        {
            DieNowWithMessage("NCP error", OT_EXIT_FAILURE);
        }
        else
        {
            DieNow(OT_EXIT_FAILURE);
        }
    }
    else if (rval == 0)
    {
        ExitNow(error = OT_ERROR_RESPONSE_TIMEOUT);
    }
    else if (errno != EINTR)
    {
        DieNowWithMessage("wait response", OT_EXIT_FAILURE);
    }

exit:
    return error;
}

otError SpiInterface::SendFrame(const uint8_t *aFrame, uint16_t aLength)
{
    otError                          error = OT_ERROR_NONE;

    VerifyOrExit(aLength < (MAX_FRAME_SIZE - HEADER_LEN), error = OT_ERROR_NO_BUFS);

    memcpy(&sSpiTxFrameBuffer[HEADER_LEN], aFrame, aLength);

    // Indicate that a frame is ready to go out.
    sSpiTxIsReady     = true;
    sSpiTxPayloadSize = aLength;

exit:
    return error;
}

} // namespace PosixApp
} // namespace ot

#endif // OPENTHREAD_POSIX_NCP_SPI_ENABLE
