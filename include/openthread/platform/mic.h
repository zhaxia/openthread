#ifndef OPENTHREAD_PLATFORM_MIC_H_
#define OPENTHREAD_PLATFORM_MIC_H_

#include <stdint.h>

#include <openthread/error.h>
#include <openthread/instance.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    OT_MIC_EVENT_SAMPLE_DONE,
    OT_MIC_EVENT_CALIBRATE_DONE,
    OT_MIC_EVENT_TEST,
} otMicEvent;

typedef void (*otPlatMicCallback)(void *aContext, otMicEvent aEvent, uint16_t *aBuffer, uint16_t aLength);

void otPlatMicInit(otInstance *aInstance, otPlatMicCallback aMicCallback, void *aContext);

otError otPlatMicSampleOneShot(otInstance *aInstance, uint16_t *aValue);

otError otPlatMicSampleStart(otInstance *aInstance, uint16_t * aBuffer, uint16_t aLength);

otError otPlatMicSample(otInstance *aInstance);

void otPlatMicSampleStop(otInstance *aInstance);

otError otPlatMicSampleCalibrate(otInstance *aInstance);

#ifdef __cplusplus
} // end of extern "C"
#endif
#endif // OPENTHREAD_PLATFORM_MIC_H_ 
