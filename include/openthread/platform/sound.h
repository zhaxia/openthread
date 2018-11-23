#ifndef OPENTHREAD_PLATFORM_SOUND_H_
#define OPENTHREAD_PLATFORM_SOUND_H_

#include <stdint.h>

#include <openthread/error.h>
#include <openthread/instance.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const uint32_t *(*otPlatSoundCallback)(void *aContext);

otError otPlatRadioInit(otInstance *aInstance, otPlatSoundCallback aNextBufferHandler, void *aContext);

otError otPlatSoundStart(otInstance *aInstance, const uint32_t *aBuffer, uint16_t aSize, uint8_t aFlags);

void otPlatSoundStop(otInstance *aInstance);

#ifdef __cplusplus
} // end of extern "C"
#endif
#endif // OPENTHREAD_PLATFORM_SOUND_H_ 
