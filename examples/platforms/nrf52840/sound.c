#include <openthread-core-config.h>
#include <openthread/config.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <utils/code_utils.h>
#include <openthread/platform/sound.h>

#include "platform-nrf5.h"

#include <nrfx_i2s.h>

#include <nrf.h>

#define MAX_BUF_SIZE 4096

#define I2S_SCK_PIN   13 // P0.13 LED1
#define I2S_LRCK_PIN  14 // P0.14 LED2
#define I2S_SDOUT_PIN 15 // P0.15 LED3
// P0.16 LED4

#define I2S_PRIORITY 7


static nrfx_i2s_buffers_t sI2sBuffer;
static nrfx_i2s_config_t  sI2sConfig;

static otPlatSoundCallback sNextBufferHandler;
static void *sContext;

void nRfI2sDataHandler(nrfx_i2s_buffers_t const * aReleasedBuffer, uint32_t aStatus);

otError nrfErrorMap(nrfx_err_t aError)
{
    otError error;

    switch (aError)
    {
    case NRFX_SUCCESS:
        error = OT_ERROR_NONE;
        break;

    case NRFX_ERROR_INVALID_STATE:
        error = OT_ERROR_INVALID_STATE;
        break;

    case NRFX_ERROR_INVALID_ADDR:
        error = OT_ERROR_PARSE;
        break;

    default:
        error = OT_ERROR_FAILED;
        break;
    }

    return error;
}

void nrf5SoundInit(void)
{
    sNextBufferHandler = NULL;
    sContext           = NULL;

    sI2sConfig.sck_pin      = I2S_SCK_PIN;
    sI2sConfig.lrck_pin     = I2S_LRCK_PIN;
    sI2sConfig.sdout_pin    = I2S_SDOUT_PIN;
    sI2sConfig.mck_pin      = NRFX_I2S_PIN_NOT_USED;
    sI2sConfig.sdin_pin     = NRFX_I2S_PIN_NOT_USED;
    sI2sConfig.irq_priority = I2S_PRIORITY;
    sI2sConfig.mode         = NRF_I2S_MODE_MASTER;
    sI2sConfig.format       = NRF_I2S_FORMAT_I2S;
    sI2sConfig.alignment    = NRF_I2S_ALIGN_LEFT;
    sI2sConfig.sample_width = NRF_I2S_SWIDTH_16BIT;
    sI2sConfig.channels     = NRF_I2S_CHANNELS_LEFT;
    //sI2sConfig.mck_setup    = NRF_I2S_MCK_32MDIV23; // 44.1kHz
    sI2sConfig.mck_setup    = NRF_I2S_MCK_32MDIV63;   // 16kHz
    sI2sConfig.ratio        = NRF_I2S_RATIO_32X;

    assert(nrfx_i2s_init(&sI2sConfig, nRfI2sDataHandler) == NRFX_SUCCESS);
}

void nrf5SoundDeinit(void)
{
    nrfx_i2s_uninit();
}

void nRfI2sDataHandler(nrfx_i2s_buffers_t const * aReleasedBuffer, uint32_t aStatus)
{
    if ((aStatus == NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED) && (sNextBufferHandler != NULL))
    {
        nrfx_i2s_buffers_t i2sBuffer;

        i2sBuffer.p_tx_buffer = sNextBufferHandler(sContext);
        i2sBuffer.p_rx_buffer = NULL;

        if (i2sBuffer.p_tx_buffer != NULL)
        {
            nrfx_i2s_next_buffers_set(&i2sBuffer);
        }
        else
        {
            nrfx_i2s_stop();
        }
    }
}

otError otPlatRadioInit(otInstance *aInstance, otPlatSoundCallback aNextBufferHandler, void *aContext)
{
    (void)aInstance;

    sNextBufferHandler = aNextBufferHandler;
    sContext           = aContext;

    return OT_ERROR_NONE;
}

otError otPlatSoundStart(otInstance *aInstance, const uint32_t *aBuffer, uint16_t aSize, uint8_t aFlags)
{
    nrfx_err_t error;

    (void)aInstance;

    sI2sBuffer.p_tx_buffer = aBuffer;
    sI2sBuffer.p_rx_buffer = NULL;

    error = nrfx_i2s_start(&sI2sBuffer, aSize, aFlags);

    return nrfErrorMap(error);
}

void otPlatSoundStop(otInstance *aInstance)
{
    (void)aInstance;

    nrfx_i2s_stop();
}

