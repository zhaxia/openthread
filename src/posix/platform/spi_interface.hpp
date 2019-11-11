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
 *   This file includes definitions for the SPI interface to radio (RCP).
 */

#ifndef POSIX_APP_SPI_INTERFACE_HPP_
#define POSIX_APP_SPI_INTERFACE_HPP_

#include "platform-config.h"
#include "spinel_interface.hpp"

#include "ncp/hdlc.hpp"
#include <openthread-system.h>

#if OPENTHREAD_POSIX_NCP_SPI_ENABLE

namespace ot {
namespace PosixApp {

#define MAX_FRAME_SIZE 2048
#define HEADER_LEN 5
#define SPI_HEADER_RESET_FLAG 0x80
#define SPI_HEADER_CRC_FLAG 0x40
#define SPI_HEADER_PATTERN_VALUE 0x02
#define SPI_HEADER_PATTERN_MASK 0x03
#define SPI_RX_ALIGN_ALLOWANCE_MAX 16

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC 1000
#endif

#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC 1000
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC (USEC_PER_MSEC * MSEC_PER_SEC)
#endif

#define SPI_POLL_PERIOD_MSEC (MSEC_PER_SEC / 30)

#define IMMEDIATE_RETRY_COUNT 5
#define FAST_RETRY_COUNT 15

#define IMMEDIATE_RETRY_TIMEOUT_MSEC 1
#define FAST_RETRY_TIMEOUT_MSEC 10
#define SLOW_RETRY_TIMEOUT_MSEC 33

#define GPIO_INT_ASSERT_STATE 0 // I̅N̅T̅ is asserted low
#define GPIO_RES_ASSERT_STATE 0 // R̅E̅S̅ is asserted low

#define SPI_RX_ALIGN_ALLOWANCE_MAX 16

#define SOCKET_DEBUG_BYTES_PER_LINE 16

#ifndef AUTO_PRINT_BACKTRACE
#define AUTO_PRINT_BACKTRACE (HAVE_EXECINFO_H)
#endif

#define AUTO_PRINT_BACKTRACE_STACK_DEPTH 20
/**
 * This class defines an SPI interface to the Radio Co-processor (RCP)
 *
 */
class SpiInterface : public SpinelInterface
{
public:
    explicit SpiInterface(Callbacks &aCallbacks);

    otError Init(otPlatformConfig *aConfig);

    void Deinit(void);

    bool IsDecoding(void) const { return mIsDecoding; }
    otError SendFrame(const uint8_t *aFrame, uint16_t aLength);

    otError WaitResponse(struct timeval &aTimeout);

    void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd,
                     struct timeval &aTimeout);

    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet);

private:
    void ResetGpioInit(const char *aPath);
    void Trigerreset(void);
    void IntGpioInit(const char *path);
    void SpiDevInit(const char *aPath, uint8_t aMode, uint32_t aSpeed);

    void spi_header_set_flag_byte(uint8_t *header, uint8_t value);
    void spi_header_set_accept_len(uint8_t *header, uint16_t len);
    void spi_header_set_data_len(uint8_t *header, uint16_t len);
    uint8_t spi_header_get_flag_byte(const uint8_t *header);
    uint16_t spi_header_get_accept_len(const uint8_t *header);
    uint16_t spi_header_get_data_len(const uint8_t *header);
    uint8_t *get_real_rx_frame_start(void);
    int do_spi_xfer(int len);
    int push_pull_spi(void);
    bool check_and_clear_interrupt(void);
    void log_debug_buffer(const char *desc, const uint8_t *buffer_ptr, int buffer_len, bool force);


    void ClearStats(void);
    void LogStats(void);

    enum
    {
        kSpiSpeed      = 1000000,  // In HZ
        kSpiMode       = 0,        // SPI Mode 0: CPOL=0, CPHA=0
        kSpiCsDelay    = 20,       // In microseconds
        kSpiResetDelay = 0,        // In microseconds
    };


    bool       mIsDecoding;
    Callbacks &mCallbacks;

    int sSpiDevFd;
    int sResGpioValueFd;
    int sIntGpioValueFd;

    int     sSpiSpeed; // in Hz (default: 1MHz)
    uint8_t sSpiMode;
    int     sSpiCsDelay; // in microseconds
    int     sSpiResetDelay;  // in milliseconds

    uint64_t sSlaveResetCount;
    uint64_t sSpiFrameCount;
    uint64_t sSpiValidFrameCount;
    uint64_t sSpiGarbageFrameCount;
    uint64_t sSpiDuplexFrameCount;
    uint64_t sSpiUnresponsiveFrameCount;
    uint64_t sSpiRxFrameCount;
    uint64_t sSpiRxFrameByteCount;
    uint64_t sSpiTxFrameCount;
    uint64_t sSpiTxFrameByteCount;

    uint16_t sSpiRxPayloadSize;
    uint8_t  sSpiRxFrameBuffer[MAX_FRAME_SIZE + SPI_RX_ALIGN_ALLOWANCE_MAX];
   
    bool     sSpiTxIsReady;
    int      sSpiTxRefusedCount;
    uint16_t sSpiTxPayloadSize;
    uint8_t  sSpiTxFrameBuffer[MAX_FRAME_SIZE + SPI_RX_ALIGN_ALLOWANCE_MAX];

    uint8_t  sSpiRxAlignAllowance;
    uint16_t sSpiSmallPacketSize;
    bool     sSlaveDidReset;
    int      sLogLevel;
    bool           did_print_rate_limit_log = false;

    Hdlc::FrameWritePointer &mWritePointer;
};

} // namespace PosixApp
} // namespace ot

#endif // OPENTHREAD_POSIX_NCP_SPI_ENABLE
#endif // POSIX_APP_SPI_INTERFACE_HPP_
