
#include "cli_wav.hpp"
#include "cli_uart.hpp"

#include "cli/cli.hpp"
#include "common/encoding.hpp"

#include <openthread/platform/alarm-micro.h>

namespace ot {
namespace Cli {

CliWav::CliWav(Interpreter &aInterpreter)
    : mInterpreter(aInterpreter)
    , mWav((const uint8_t*)kFlashWavStart, (const uint8_t*)kFlashWavEnd)
    , mPool()
    , mTimer(*aInterpreter.mInstance, &CliWav::HandleTimer, this)
{
   otPlatRadioInit(mInterpreter.mInstance, HandleSoundCallback, this);
   otPlatMicInit(mInterpreter.mInstance, HandleMicCallback, this);
   memset(mTempBuffer, 0, sizeof(mTempBuffer));
}

CliWav &CliWav::GetOwner(OwnerLocator &aOwnerLocator)
{
    CliWav &cliWav = Uart::sUartServer->GetInterpreter().GetCliWav();
    OT_UNUSED_VARIABLE(aOwnerLocator);

   return cliWav;
}

void CliWav::HandleTimer(Timer &aTimer)
{
    GetOwner(aTimer).HandleTimer();
}

void CliWav::HandleTimer()
{
    uint32_t  buffer[kWavBufferSize];
    otError   error      = OT_ERROR_NONE;
    uint32_t  readLength = static_cast<uint32_t>(kWavBufferSize * sizeof(uint32_t));

    VerifyOrExit(mSoundRetries < kSoundRetries, error = OT_ERROR_FAILED);
    if (mWav.Read(mWavOffset, reinterpret_cast<uint8_t*>(buffer), readLength) == 0 && mSoundRetries < kSoundRetries)
    {
        mInterpreter.mServer->OutputFormat("Repeat: %d\r\n", mSoundRetries);

        mWavOffset = 0;
        mSoundRetries++;

        VerifyOrExit(mSoundRetries < kSoundRetries, error = OT_ERROR_FAILED);
        VerifyOrExit(mWav.Read(mWavOffset, reinterpret_cast<uint8_t*>(buffer), readLength) != 0, error = OT_ERROR_FAILED);
    }

    VerifyOrExit(mPool.In(buffer) == OT_ERROR_NONE, error = OT_ERROR_NO_BUFS);

    mWavOffset += kWavBufferSize * sizeof(uint32_t);

    if (mWavOffset % 0x20000 == 0)
    {
        mInterpreter.mServer->OutputFormat("%x\r\n", mWavOffset);
    }

exit:
    if (error == OT_ERROR_NONE || error == OT_ERROR_NO_BUFS)
    {
        mTimer.StartAt(mTimer.GetFireTime(), kReadInterval);
    }

    if ((error == OT_ERROR_NONE) && !mSoundRunning && mPool.IsFull())
    {
        mInterpreter.mServer->OutputFormat("sound start\r\n");

        mSoundRunning = true;
        otPlatSoundStart(mInterpreter.mInstance, mPool.Out(), kWavBufferSize, 0);
    }

    if (error == OT_ERROR_FAILED)
    {
        mSoundRunning = false;
        mInterpreter.mServer->OutputFormat("sound stop\r\n");
    }

    return;
}

const uint32_t *CliWav::HandleSoundCallback(void *aContext)
{
    return static_cast<CliWav *>(aContext)->HandleSoundCallback();
}

const uint32_t *CliWav::HandleSoundCallback(void)
{
    if (!mPool.IsEmpty())
    {
        return mPool.Out();
    }
    else
    {
        mInterpreter.mServer->OutputFormat("mPool.IsEmpty() Running=%d\r\n", mSoundRunning);
        return mSoundRunning? mTempBuffer : NULL;
    }
}

void CliWav::HandleMicCallback(void *aContext, otMicEvent aEvent, uint16_t *aBuffer, uint16_t aLength)
{
    static_cast<CliWav *>(aContext)->HandleMicCallback(aEvent, aBuffer, aLength);
}

void CliWav::HandleMicCallback(otMicEvent aEvent, uint16_t *aBuffer, uint16_t aLength)
{
    if (aEvent == OT_MIC_EVENT_SAMPLE_DONE)
    {
        mInterpreter.mServer->OutputFormat("%4d %4d %4d %4d\r\n",
                                           static_cast<int16_t>(aBuffer[0]),
                                           static_cast<int16_t>(aBuffer[1]),
                                           static_cast<int16_t>(aBuffer[2]),
                                           static_cast<int16_t>(aBuffer[3]));

        for (uint16_t i = 0; i < aLength; i++)
        {
            aBuffer[i] = aBuffer[i] << 4;
        }

        mPool.In(reinterpret_cast<uint32_t *>(aBuffer));
        if (mMicRunning && !mSoundRunning && mPool.IsFull())
        {
            mSoundRunning = true;
            mInterpreter.mServer->OutputFormat("Mic Sound Start\r\n");
            otPlatSoundStart(mInterpreter.mInstance, mPool.Out(), kWavBufferSize, 0);
        }

        otPlatMicSampleStart(mInterpreter.mInstance, aBuffer, aLength);
    }
    else if (aEvent == OT_MIC_EVENT_CALIBRATE_DONE)
    {
        otPlatMicSampleStart(mInterpreter.mInstance, mMicBuffer[0], kMicBufferSize);
        otPlatMicSampleStart(mInterpreter.mInstance, mMicBuffer[1], kMicBufferSize);
        otPlatMicSample(mInterpreter.mInstance);

        mInterpreter.mServer->OutputFormat("ADC Calibrate Done\r\n");
    }
}

otError CliWav::Process(int argc, char *argv[])
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(argc > 0, error = OT_ERROR_INVALID_ARGS);

    if (strcmp(argv[0], "sound") == 0)
    {
        VerifyOrExit(argc > 1, error = OT_ERROR_INVALID_ARGS);

        if (strcmp(argv[1], "show") == 0)
        {
            char             str[5];
            const WavHeader *wavHeader;
            const WavFmt *   wavFmt;
            const WavData *  wavData;
         
            VerifyOrExit(mWav.IsValid(), error = OT_ERROR_INVALID_STATE);
         
            if ((wavHeader = mWav.GetWavHeader()) != NULL)
            {
                mInterpreter.mServer->OutputFormat("RiffId       : %s\r\n", wavHeader->GetRiffIdStr(str));
                mInterpreter.mServer->OutputFormat("ChunkSize    : %d\r\n", wavHeader->GetChunkSize());
                mInterpreter.mServer->OutputFormat("RiffFormat   : %s\r\n\r\n", wavHeader->GetRiffFormatStr(str));
            }
         
            if ((wavFmt = mWav.GetWavFmt()) != NULL)
            {
                mInterpreter.mServer->OutputFormat("SubChunkId   : %s\r\n", wavFmt->GetSubChunkIdStr(str));
                mInterpreter.mServer->OutputFormat("SubChunkSize : %d\r\n", wavFmt->GetSubChunkSize());
                mInterpreter.mServer->OutputFormat("FormatTag    : 0x%0X\r\n", wavFmt->GetFormatTag());
                mInterpreter.mServer->OutputFormat("NumChannels  : %d\r\n", wavFmt->GetNumChannels());
                mInterpreter.mServer->OutputFormat("SamplesPerSec: %d\r\n", wavFmt->GetSamplesPerSec());
                mInterpreter.mServer->OutputFormat("BytesPerSec  : %d\r\n", wavFmt->GetAvgBytesPerSec());
                mInterpreter.mServer->OutputFormat("BlockAlign   : %d\r\n", wavFmt->GetBlockAlign());
                mInterpreter.mServer->OutputFormat("BitsPerSample: %d\r\n\r\n", wavFmt->GetBitsPerSample());
            }
         
            if ((wavData = mWav.GetWavData()) != NULL)
            {
                const uint8_t *data;
                const int16_t *data16;
         
                mInterpreter.mServer->OutputFormat("SubChunkId   : %s\r\n", wavData->GetSubChunkIdStr(str));
                mInterpreter.mServer->OutputFormat("SubChunkSize : %d\r\n", wavData->GetSubChunkSize());
                data = wavData->GetData();
                mInterpreter.mServer->OutputFormat("Data(U8)     : ");
         
                while ((*data == 0) || (*data == 0xFF))
                {
                    data++;
                }
         
                for (uint8_t i = 0; i < 16; i++)
                {
                    mInterpreter.mServer->OutputFormat("%02X ", data[i]);
                }
                mInterpreter.mServer->OutputFormat("\r\n");
         
                data16 = reinterpret_cast<const int16_t*>(data);
                mInterpreter.mServer->OutputFormat("Data(I16)    : ");
                for (uint8_t i = 0; i < 16; i++)
                {
                    mInterpreter.mServer->OutputFormat("%d ", data16[i]);
                }
                mInterpreter.mServer->OutputFormat("\r\n");
            }
        }
        else if (strcmp(argv[1], "start") == 0)
        {
            uint32_t payloadOffset = mWav.GetWavDataPayloadOffset();

            VerifyOrExit(payloadOffset != Wav::kInvalidOffset);
            mWav.SetDataPayloadOffset(payloadOffset);
     
            mWavOffset    = 0;
            mBufferIndex  = 0;
            mSoundRunning = false;
            mSoundRetries = 0;

            mTimer.Start(kReadInterval);
        }
        else if (strcmp(argv[1], "stop") == 0)
        {
            mSoundRunning = false;
            mSoundRetries = kSoundRetries;
        }
        else
        {
            error = OT_ERROR_INVALID_ARGS;
        }
    }
    else if (strcmp(argv[0], "debug") == 0)
    {
        mInterpreter.mServer->OutputFormat("debug\r\n");
    }
    else if (strcmp(argv[0], "mic") == 0)
    {
        VerifyOrExit(argc > 1, error = OT_ERROR_INVALID_ARGS);
        
        if (strcmp(argv[1], "conv") == 0)
        {
            if (argc > 2)
            {
                if (strcmp(argv[2], "0") == 0)
                {
                    error = otPlatMicSampleStart(mInterpreter.mInstance, mMicBuffer[0], kMicBufferSize);
                    mMicStart = otPlatAlarmMicroGetNow();
                }
                else if (strcmp(argv[2], "1") == 0)
                {
                    error = otPlatMicSampleStart(mInterpreter.mInstance, mMicBuffer[1], kMicBufferSize);
                    mMicStart = otPlatAlarmMicroGetNow();
                }
                else
                {
                    error = OT_ERROR_INVALID_ARGS;
                }
            }
            else
            {
                error = otPlatMicSampleStart(mInterpreter.mInstance, mMicBuffer[0], kMicBufferSize);
                mMicStart = otPlatAlarmMicroGetNow();
            }
        }
        else if (strcmp(argv[1], "sample") == 0)
        {
            error = otPlatMicSample(mInterpreter.mInstance);
            mMicStart = otPlatAlarmMicroGetNow();
        }
        else if (strcmp(argv[1], "one") == 0)
        {
            uint16_t value;

            error = otPlatMicSampleOneShot(mInterpreter.mInstance, &value);
            mInterpreter.mServer->OutputFormat("Sample Value: %d\r\n", value);
        }
        else if (strcmp(argv[1], "start") == 0)
        {
            mMicRunning = true;
            error = otPlatMicSampleCalibrate(mInterpreter.mInstance);
        }
        else if (strcmp(argv[1], "stop") == 0)
        {
            mMicRunning   = false;
            mSoundRunning = false;
            otPlatMicSampleStop(mInterpreter.mInstance);
        }
        else if (strcmp(argv[1], "cal") == 0)
        {
            error = otPlatMicSampleCalibrate(mInterpreter.mInstance);
        }
        else
        {
            error = OT_ERROR_INVALID_ARGS;
        }
    }
    else
    {
        error = OT_ERROR_INVALID_ARGS;
    }

exit:
    return error;
}


}  // namespace Cli
}  // namespace ot
