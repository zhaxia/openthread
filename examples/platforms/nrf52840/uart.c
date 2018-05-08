/*
 *  Copyright (c) 2016, The OpenThread Authors.
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
 *   This file implements the OpenThread platform abstraction for UART communication.
 *
 */

#include <openthread-core-config.h>
#include <openthread/config.h>

#include <stddef.h>
#include <stdint.h>

#include <utils/code_utils.h>
#include <openthread/types.h>
#include <openthread/platform/toolchain.h>
#include <openthread/platform/uart.h>

#include "platform.h"

#include "platform-nrf5.h"
#include <drivers/clock/nrf_drv_clock.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_uart.h>

#if (USB_CDC_AS_SERIAL_TRANSPORT == 0)

bool sUartEnabled = false;

/**
 *  UART TX buffer variables.
 */
static const uint8_t *sTransmitBuffer = NULL;
static uint16_t       sTransmitLength = 0;
static bool           sTransmitDone   = 0;

/**
 *  UART RX ring buffer variables.
 */
static uint8_t  sReceiveBuffer[UART_RX_BUFFER_SIZE];
static uint16_t sReceiveHead = 0;
static uint16_t sReceiveTail = 0;

/**
 * Function for checking if RX buffer is full.
 *
 * @retval true  RX buffer is full.
 * @retval false RX buffer is not full.
 */
static __INLINE bool isRxBufferFull()
{
    uint16_t next = (sReceiveHead + 1) % UART_RX_BUFFER_SIZE;
    return (next == sReceiveTail);
}

/**
 * Function for checking if RX buffer is empty.
 *
 * @retval true  RX buffer is empty.
 * @retval false RX buffer is not empty.
 */
static __INLINE bool isRxBufferEmpty()
{
    return (sReceiveHead == sReceiveTail);
}

/**
 * Function for notifying application about new bytes received.
 */
static void processReceive(void)
{
    // Set head position to not be changed during read procedure.
    uint16_t head = sReceiveHead;

    otEXPECT(isRxBufferEmpty() == false);

    // In case head roll back to the beginning of the buffer, notify about left
    // bytes from the end of the buffer.
    if (head < sReceiveTail)
    {
        otPlatUartReceived(&sReceiveBuffer[sReceiveTail], (UART_RX_BUFFER_SIZE - sReceiveTail));
        sReceiveTail = 0;
    }

    // Notify about received bytes.
    if (head > sReceiveTail)
    {
        otPlatUartReceived(&sReceiveBuffer[sReceiveTail], (head - sReceiveTail));
        sReceiveTail = head;
    }

exit:
    return;
}

/**
 * Function for notifying application about transmission being done.
 */
static void processTransmit(void)
{
    otEXPECT(sTransmitBuffer != NULL);

    if (sTransmitDone)
    {
        // Clear Transmition transaction and notify application.
        sTransmitBuffer = NULL;
        sTransmitLength = 0;
        sTransmitDone   = false;
        otPlatUartSendDone();
    }

exit:
    return;
}

void nrf5UartProcess(void)
{
    processReceive();
    processTransmit();
}

void nrf5UartInit(void)
{
    // Intentionally empty.
}

void nrf5UartDeinit(void)
{
    if (sUartEnabled)
    {
        otPlatUartDisable();
    }
}

otError otPlatUartEnable(void)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(sUartEnabled == false, error = OT_ERROR_ALREADY);

    // Set up TX and RX pins.
    nrf_gpio_pin_set(UART_PIN_TX);
    nrf_gpio_cfg_output(UART_PIN_TX);
    nrf_gpio_cfg_input(UART_PIN_RX, NRF_GPIO_PIN_NOPULL);
    nrf_uart_txrx_pins_set(UART_INSTANCE, UART_PIN_TX, UART_PIN_RX);

#if (UART_HWFC == NRF_UART_HWFC_ENABLED)
    // Set up CTS and RTS pins.
    nrf_gpio_cfg_input(UART_PIN_CTS, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_pin_set(UART_PIN_RTS);
    nrf_gpio_cfg_output(UART_PIN_RTS);
    nrf_uart_hwfc_pins_set(UART_INSTANCE, UART_PIN_RTS, UART_PIN_CTS);
#endif

    // Configure baudrate.
    nrf_uart_baudrate_set(UART_INSTANCE, UART_BAUDRATE);

    // Configure parity and hardware flow control.
    nrf_uart_configure(UART_INSTANCE, UART_PARITY, UART_HWFC);

    // Clear UART specific events.
    nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_TXDRDY);
    nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_ERROR);
    nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_RXDRDY);

    // Enable interrupts for TX.
    nrf_uart_int_enable(UART_INSTANCE, NRF_UART_INT_MASK_TXDRDY);

    // Enable interrupts for RX.
    nrf_uart_int_enable(UART_INSTANCE, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);

    // Configure NVIC to handle UART interrupts.
    NVIC_SetPriority(UART_IRQN, UART_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(UART_IRQN);
    NVIC_EnableIRQ(UART_IRQN);

    // Start HFCLK
    nrf_drv_clock_hfclk_request(NULL);

    while (!nrf_drv_clock_hfclk_is_running())
    {
    }

    // Enable UART instance, and start RX on it.
    nrf_uart_enable(UART_INSTANCE);
    nrf_uart_task_trigger(UART_INSTANCE, NRF_UART_TASK_STARTRX);

    sUartEnabled = true;

exit:
    return error;
}

otError otPlatUartDisable(void)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(sUartEnabled == true, error = OT_ERROR_ALREADY);

    // Disable NVIC interrupt.
    NVIC_DisableIRQ(UART_IRQN);
    NVIC_ClearPendingIRQ(UART_IRQN);
    NVIC_SetPriority(UART_IRQN, 0);

    // Disable interrupts for TX.
    nrf_uart_int_disable(UART_INSTANCE, NRF_UART_INT_MASK_TXDRDY);

    // Disable interrupts for RX.
    nrf_uart_int_disable(UART_INSTANCE, NRF_UART_INT_MASK_RXDRDY | NRF_UART_INT_MASK_ERROR);

    // Disable UART instance.
    nrf_uart_disable(UART_INSTANCE);

    // Release HF clock.
    nrf_drv_clock_hfclk_release();

    sUartEnabled = false;

exit:
    return error;
}

otError otPlatUartSend(const uint8_t *aBuf, uint16_t aBufLength)
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(sTransmitBuffer == NULL, error = OT_ERROR_BUSY);

    // Set up transmit buffer and its size without counting first triggered byte.
    sTransmitBuffer = aBuf;
    sTransmitLength = aBufLength - 1;

    // Initiate Transmission process.
    nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_TXDRDY);
    nrf_uart_txd_set(UART_INSTANCE, *sTransmitBuffer++);
    nrf_uart_task_trigger(UART_INSTANCE, NRF_UART_TASK_STARTTX);

exit:
    return error;
}

/**
 * Interrupt handler of UART0 peripherial.
 */
void UARTE0_UART0_IRQHandler(void)
{
    // Check if any error has been detected.
    if (nrf_uart_event_check(UART_INSTANCE, NRF_UART_EVENT_ERROR))
    {
        // Clear error event and ignore erronous byte in RXD register.
        nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_ERROR);
        nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_RXDRDY);
    }
    else if (nrf_uart_event_check(UART_INSTANCE, NRF_UART_EVENT_RXDRDY))
    {
        // Clear RXDRDY event.
        nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_RXDRDY);

        // Read byte from the UART buffer.
        uint8_t byte = nrf_uart_rxd_get(UART_INSTANCE);

        if (!isRxBufferFull())
        {
            sReceiveBuffer[sReceiveHead] = byte;
            sReceiveHead                 = (sReceiveHead + 1) % UART_RX_BUFFER_SIZE;
            PlatformEventSignalPending();
        }
    }

    if (nrf_uart_event_check(UART_INSTANCE, NRF_UART_EVENT_TXDRDY))
    {
        // Clear TXDRDY event.
        nrf_uart_event_clear(UART_INSTANCE, NRF_UART_EVENT_TXDRDY);

        // Send any more bytes if available or call application about TX done.
        if (sTransmitLength)
        {
            nrf_uart_txd_set(UART_INSTANCE, *sTransmitBuffer++);
            sTransmitLength--;
        }
        else
        {
            sTransmitDone = true;
            nrf_uart_task_trigger(UART_INSTANCE, NRF_UART_TASK_STOPTX);
            PlatformEventSignalPending();
        }
    }
}

#endif // USB_CDC_AS_SERIAL_TRANSPORT == 0

/**
 * The UART driver weak functions definition.
 *
 */
OT_TOOL_WEAK void otPlatUartSendDone(void)
{
}

OT_TOOL_WEAK void otPlatUartReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    (void)aBuf;
    (void)aBufLength;
}



//-------------------------------------------------
#include <drivers/include/nrfx_gpiote.h>
#define GPIOTE_IRQN                   GPIOTE_IRQn
#define GPIOTE_IRQ_PRIORITY           7  // NRFX_GPIOTE_CONFIG_IRQ_PRIORITY

#define SYNC_PIN_EVENT_CHANNEL        NRF_GPIOTE_EVENTS_IN_1  // 
#define SYNC_PIN_EVENT_MASK           NRF_GPIOTE_INT_IN1_MASK // 1 << 1
#define SYNC_PIN                      11   // BUTTON1   P0.11 default High level
#define LED1_PIN                      13   // LED1      P0.13

/**
 * @brief Function for configuring: SYNC_PIN pin for input, LED1_PIN pin for output,
 * and configures GPIOTE to give an interrupt on pin change.
 */
GpioEventHandler mGpioEventHandler = NULL;
void * mGpioEventHandlerContext = NULL;

/**
 * Interrupt handler of GPIOTE peripherial.
 */
void GPIOTE_IRQHandler(void)
{
    uint32_t input[GPIO_COUNT] = {0};

    if (nrf_gpiote_event_is_set(NRF_GPIOTE_EVENTS_PORT) &&
        nrf_gpiote_int_is_enabled(NRF_GPIOTE_INT_PORT_MASK))
    {
        nrf_gpiote_event_clear(NRF_GPIOTE_EVENTS_PORT);
        nrf_gpio_ports_read(0, GPIO_COUNT, input);
        if (!nrf_gpio_pin_read(SYNC_PIN))
        {
            if (mGpioEventHandler != NULL)
            {
                nrf_gpio_pin_toggle(LED1_PIN);
                mGpioEventHandler(mGpioEventHandlerContext);
            }
        }
    }
}

typedef enum {
    SYNC_PIN_MODE_UNINIT,
    SYNC_PIN_MODE_MASTER,
    SYNC_PIN_MODE_SLAVE,
} SyncPinMode;

SyncPinMode mSyncPinMode = SYNC_PIN_MODE_UNINIT;

void otPlatSyncPinUninit()
{
    if (mSyncPinMode == SYNC_PIN_MODE_UNINIT)
    {
        return;
    }

    if (mSyncPinMode == SYNC_PIN_MODE_SLAVE)
    {
        nrf_gpio_cfg_sense_set(SYNC_PIN, NRF_GPIO_PIN_NOSENSE);

        NVIC_DisableIRQ(GPIOTE_IRQN);
        NVIC_ClearPendingIRQ(GPIOTE_IRQN);
        NVIC_SetPriority(GPIOTE_IRQN, 0);

        nrf_gpiote_int_disable(NRF_GPIOTE_INT_PORT_MASK);
        nrf_gpiote_event_clear(NRF_GPIOTE_EVENTS_PORT);
        mGpioEventHandler = NULL;
    }

    nrf_gpio_cfg_default(SYNC_PIN);
    mSyncPinMode = SYNC_PIN_MODE_UNINIT;
}

void otPlatSyncPinSlaveInit(GpioEventHandler aGpioEventHandler, void *aContext)
{
    if (mSyncPinMode == SYNC_PIN_MODE_SLAVE)
    {
        return;
    }

    NVIC_SetPriority(GPIOTE_IRQN, GPIOTE_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(GPIOTE_IRQN);
    NVIC_EnableIRQ(GPIOTE_IRQN);

    nrf_gpiote_event_clear(NRF_GPIOTE_EVENTS_PORT);
    nrf_gpiote_int_enable(NRF_GPIOTE_INT_PORT_MASK);

    nrf_gpio_cfg_input(SYNC_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpiote_event_configure(SYNC_PIN_EVENT_CHANNEL, SYNC_PIN, NRF_GPIOTE_POLARITY_HITOLO);
    nrf_gpio_cfg_sense_set(SYNC_PIN, NRF_GPIO_PIN_SENSE_LOW);

    mSyncPinMode = SYNC_PIN_MODE_SLAVE;
    mGpioEventHandler = aGpioEventHandler;
    mGpioEventHandlerContext = aContext;
}

void otPlatSyncPinMasterInit()
{
    if (mSyncPinMode == SYNC_PIN_MODE_MASTER)
    {
        return;
    }

    otPlatSyncPinUninit();

    nrf_gpio_pin_set(SYNC_PIN);
    nrf_gpio_cfg_output(SYNC_PIN);
    mSyncPinMode = SYNC_PIN_MODE_MASTER;
}

otError otPlatSyncPinToggle()
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(mSyncPinMode == SYNC_PIN_MODE_MASTER, error = OT_ERROR_INVALID_STATE);
    nrf_gpio_pin_toggle(SYNC_PIN);

exit:
    return error;
}

otError otPlatSyncPinSet()
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(mSyncPinMode == SYNC_PIN_MODE_MASTER, error = OT_ERROR_INVALID_STATE);
    nrf_gpio_pin_set(SYNC_PIN);

exit:
    return error;
}

otError otPlatSyncPinClear()
{
    otError error = OT_ERROR_NONE;

    otEXPECT_ACTION(mSyncPinMode == SYNC_PIN_MODE_MASTER, error = OT_ERROR_INVALID_STATE);
    nrf_gpio_pin_clear(SYNC_PIN);

exit:
    return error;
}

void otPlatLedPinInit()
{
    nrf_gpio_pin_set(LED1_PIN);
    nrf_gpio_cfg_output(LED1_PIN);
}

void otPlatLedPinToggle()
{
    nrf_gpio_pin_toggle(LED1_PIN);
}

void otPlatLedPinSet()
{
    nrf_gpio_pin_set(LED1_PIN);
}

void otPlatLedPinClear()
{
    nrf_gpio_pin_clear(LED1_PIN);
}
