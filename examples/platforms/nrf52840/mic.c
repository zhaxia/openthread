#include <openthread-core-config.h>
#include <openthread/config.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <utils/code_utils.h>
#include <openthread/platform/mic.h>

#include "platform-nrf5.h"

#include <nrfx_saadc.h>

#include <nrf.h>

#define MAX_BUF_SIZE 4096

#define NRF_MIC_AIN_CHANNEL 1   // AIN1, A1, P0.04
#define NRF_MIC_AIN_PIN     4   // AIN1, A1, P0.04

#define NRF_MIC_PRIORITY 6

#define NRF_MIC_SAMPLE_RATE         16000//22050//44100
#define NRF_MIC_SAMPLE_RATE_CAP_CMP (16000000U / NRF_MIC_SAMPLE_RATE)
// 16MHz / 363 = 44.077kHz

void nrfSaadcEventHandler(nrfx_saadc_evt_t const *aEvent);

static otPlatMicCallback sMicCallback = NULL;
static void *sContext = NULL;

static const nrf_saadc_channel_config_t cChannelConfig =
{
    .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
    .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
    //.gain       = NRF_SAADC_GAIN1_4,
    //.resistor_p = NRF_SAADC_RESISTOR_PULLDOWN,
    //.resistor_n = NRF_SAADC_RESISTOR_PULLDOWN,
    .gain       = NRF_SAADC_GAIN1_4,
    .reference  = NRF_SAADC_REFERENCE_INTERNAL,  // Ref = 0.6v
    .acq_time   = NRF_SAADC_ACQTIME_10US,
    .mode       = NRF_SAADC_MODE_SINGLE_ENDED,
    .burst      = NRF_SAADC_BURST_DISABLED,
    .pin_p      = NRF_SAADC_INPUT_AIN1,
    .pin_n      = NRF_SAADC_INPUT_DISABLED,
};

void nrf5MicInit(void)
{
    nrfx_err_t                 error;
    nrfx_saadc_config_t        saadcConfig;
    
    saadcConfig.resolution                    = NRF_SAADC_RESOLUTION_12BIT;
    saadcConfig.oversample                    = NRF_SAADC_OVERSAMPLE_DISABLED;
    saadcConfig.sample_rate.mode              = NRF_SAADC_SAMPLE_RATE_MODE_TIMER;
    saadcConfig.sample_rate.cap_and_cmp_value = NRF_MIC_SAMPLE_RATE_CAP_CMP;
    saadcConfig.interrupt_priority            = NRF_MIC_PRIORITY;
    saadcConfig.low_power_mode                = false;
    
    error =  nrfx_saadc_init(&saadcConfig, nrfSaadcEventHandler);
    assert(error == NRFX_SUCCESS);

    error = nrfx_saadc_channel_init(NRF_MIC_AIN_CHANNEL, &cChannelConfig);
}

void nrf5MicDeinit(void)
{
    nrfx_saadc_channel_uninit(NRF_MIC_AIN_CHANNEL);
    nrfx_saadc_uninit();
}

void otPlatMicInit(otInstance *aInstance, otPlatMicCallback aMicCallback, void *aContext)
{
    (void)aInstance;

    sMicCallback = aMicCallback;
    sContext     = aContext;
}

otError otPlatMicSampleOneShot(otInstance *aInstance, uint16_t *aValue)
{
    nrfx_err_t error;

    (void)aInstance;

    error = nrfx_saadc_sample_convert(NRF_MIC_AIN_CHANNEL, (nrf_saadc_value_t *)aValue);

    return (error == NRFX_SUCCESS) ? OT_ERROR_NONE : OT_ERROR_FAILED;
}

otError otPlatMicSampleStart(otInstance *aInstance, uint16_t * aBuffer, uint16_t aLength)
{
    nrfx_err_t error;

    (void)aInstance;

    error = nrfx_saadc_buffer_convert((nrf_saadc_value_t *)aBuffer, aLength);

    return (error == NRFX_SUCCESS) ? OT_ERROR_NONE : OT_ERROR_FAILED;
}

otError otPlatMicSample(otInstance *aInstance)
{
    nrfx_err_t error;

    error = nrfx_saadc_sample();
    return (error == NRFX_SUCCESS) ? OT_ERROR_NONE : OT_ERROR_FAILED;
}

void otPlatMicSampleStop(otInstance *aInstance)
{
    (void)aInstance;

    nrfx_saadc_abort();
}


otError otPlatMicSampleCalibrate(otInstance *aInstance)
{
    nrfx_err_t error;

    (void)aInstance;

    error =nrfx_saadc_calibrate_offset();

    return (error == NRFX_SUCCESS) ? OT_ERROR_NONE : OT_ERROR_FAILED;
}

void nrfSaadcEventHandler(nrfx_saadc_evt_t const *aEvent)
{
    switch (aEvent->type)
    {
    case NRFX_SAADC_EVT_DONE:
        if (sMicCallback)
        {
            sMicCallback(sContext, OT_MIC_EVENT_SAMPLE_DONE, (uint16_t *)aEvent->data.done.p_buffer,
                                aEvent->data.done.size);
        }
        break;

    case NRFX_SAADC_EVT_CALIBRATEDONE:
        if (sMicCallback)
        {
            sMicCallback(sContext, OT_MIC_EVENT_CALIBRATE_DONE, NULL, 0);
        }
        break;

    case NRFX_SAADC_EVT_TEST:
        if (sMicCallback)
        {
            sMicCallback(sContext, OT_MIC_EVENT_TEST, NULL, 0);
        }
        break;

    default:
        break;
    }
}




