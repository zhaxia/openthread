
#ifndef CLI_WAV_HPP_
#define CLI_WAV_HPP_

#include <stdlib.h>
#include <string.h>

#include <openthread/instance.h>

#include "common/code_utils.hpp"
#include "common/timer.hpp"

#include <openthread/platform/mic.h>
#include <openthread/platform/sound.h>

namespace ot {
namespace Cli {

class Interpreter;
class SubChunkHeader;

#define WAV_RIFF_ID       0x46464952  // "RIFF"
#define WAV_RIFF_FORMAT   0x45564157  // "WAVE"
#define WAV_CHUNK_ID_FMT  0x20746D66  // "fmt "
#define WAV_CHUNK_ID_DATA 0x61746164  // "data"

OT_TOOL_PACKED_BEGIN
class WavHeader
{
public:
    void SetRiffId(uint32_t aRiffId) { mRiffId = aRiffId; }
    uint32_t GetRiffId(void) const { return mRiffId; }

    char *GetRiffIdStr(char *aBuf) const
    {
        *reinterpret_cast<uint32_t*>(aBuf) = mRiffId;
        aBuf[4] = '\0';

        return aBuf;
    } 

    void SetChunkSize(uint32_t aSize) { mChunkSize = aSize; }
    uint32_t GetChunkSize(void) const { return mChunkSize; }

    void SetRiffFormat(uint32_t aRiffFormat) { mRiffFormat = aRiffFormat; }
    uint32_t GetRiffFormat(void) const { return mRiffFormat; }

    char *GetRiffFormatStr(char *aBuf) const
    {
        *reinterpret_cast<uint32_t*>(aBuf) = mRiffFormat;
        aBuf[4] = '\0';

        return aBuf;
    } 

    bool IsValid(void) const { return (mRiffId == WAV_RIFF_ID) && (mRiffFormat == WAV_RIFF_FORMAT); }

    const SubChunkHeader *GetFirstSubChunkHeader(void) const
    {
        return reinterpret_cast<const SubChunkHeader *>(reinterpret_cast<const uint8_t*>(this) + sizeof(*this));
    }

private:
    uint32_t mRiffId;
    uint32_t mChunkSize;
    uint32_t mRiffFormat;
} OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
class SubChunkHeader
{
public:
    void SetSubChunkId(uint32_t aSubChunkId) { mSubChunkId = aSubChunkId; }
    uint32_t GetSubChunkId(void) const { return mSubChunkId; }

    char *GetSubChunkIdStr(char *aBuf) const
    {
        *reinterpret_cast<uint32_t*>(aBuf) = mSubChunkId;
        aBuf[4] = '\0';

        return aBuf;
    } 

    void SetSubChunkSize(uint32_t aSize) { mSubChunkSize = aSize; }
    uint32_t GetSubChunkSize(void) const { return mSubChunkSize; }

    const SubChunkHeader *GetNext(void) const
    {
        return reinterpret_cast<const SubChunkHeader *>(reinterpret_cast<const uint8_t*>(this) + sizeof(*this) + mSubChunkSize);
    }

private:
    uint32_t mSubChunkId;
    uint32_t mSubChunkSize;
} OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
class WavFmt: public SubChunkHeader
{
public:
    void Init(void)
    {
        SetSubChunkId(WAV_CHUNK_ID_FMT);
        SetSubChunkSize(sizeof(*this) - sizeof(SubChunkHeader));
    }

    uint16_t GetFormatTag(void) const { return mFormatTag; }
    uint16_t GetNumChannels(void) const { return mNumChannels; }
    uint32_t GetSamplesPerSec(void) const { return mSamplesPerSec; }
    uint32_t GetAvgBytesPerSec(void) const { return mAvgBytesPerSec; }
    uint16_t GetBlockAlign(void) const { return mBlockAlign; }
    uint16_t GetBitsPerSample(void) const { return mBitsPerSample; }

private:
    uint16_t mFormatTag;
    uint16_t mNumChannels;
    uint32_t mSamplesPerSec;
    uint32_t mAvgBytesPerSec;
    uint16_t mBlockAlign;
    uint16_t mBitsPerSample;
} OT_TOOL_PACKED_END;

OT_TOOL_PACKED_BEGIN
class WavData: public SubChunkHeader
{
public:
    void Init(void)
    {
        SetSubChunkId(WAV_CHUNK_ID_DATA);
    }

    const uint8_t *GetData(void) const { return reinterpret_cast<const uint8_t *>(this) + sizeof(*this);}
} OT_TOOL_PACKED_END;

class Wav
{
public:
    enum
    {
        kInvalidOffset = 0xffffffff,
    };

    Wav(const uint8_t *aStart, const uint8_t *aEnd)
        : mStart(aStart)
        , mOffset(aStart)
        , mEnd(aEnd)
    {
    }

    void Init(const uint8_t *aStart, uint32_t aLength)
    {
        mStart  = aStart;
        mOffset = aStart;
        mEnd    = aStart + aLength;
    }

    const WavHeader *GetWavHeader(void) const { return reinterpret_cast<const WavHeader *>(mStart); }

    const SubChunkHeader* GetSubChunkHeder(uint32_t aSubChunkId) const
    {
        const SubChunkHeader* header = GetWavHeader()->GetFirstSubChunkHeader();

        while (reinterpret_cast<const uint8_t*>(header) < mEnd)
        {
            if (header->GetSubChunkId() == aSubChunkId)
            {
                ExitNow();
            }

            header = header->GetNext();
        }

        header = NULL;

exit:
        return header;
    }

    const WavFmt *GetWavFmt(void) const
    {
        return static_cast<const WavFmt *>(GetSubChunkHeder(WAV_CHUNK_ID_FMT));
    }

    const WavData *GetWavData(void) const
    {
        return static_cast<const WavData *>(GetSubChunkHeder(WAV_CHUNK_ID_DATA));
    }


    bool IsValid(void)
    {
        bool             ret;
        const WavHeader *header = GetWavHeader();

        VerifyOrExit(header != NULL, ret = false);

        ret = header->IsValid();

exit:
        return ret;
    }

    uint32_t GetWavDataPayloadOffset()
    {
        uint32_t       offset = kInvalidOffset;
        const WavData *wavData;
        const uint8_t *dataPayload;

        wavData = GetWavData();
        VerifyOrExit(wavData != NULL);

        dataPayload = wavData->GetData();
        VerifyOrExit(dataPayload != NULL);
        VerifyOrExit(mStart < dataPayload && dataPayload < mEnd);

        offset = dataPayload - mStart;
exit:
        return offset;
    }

    otError SetDataPayloadOffset(uint32_t aOffset)
    {
        otError error = OT_ERROR_NONE;

        VerifyOrExit(mStart + aOffset < mEnd, error = OT_ERROR_INVALID_ARGS);

exit:
        return error;
    }

    uint32_t Read(uint32_t aOffset, uint8_t *aBuf, uint32_t aLength)
    {
        uint32_t readLength = 0;

        VerifyOrExit(mStart + mPayloadOffset + aOffset < mEnd);

        memcpy(aBuf, mStart + mPayloadOffset + aOffset, aLength);
        readLength = aLength;

exit:
        return readLength;
    }

private:
    uint32_t       mPayloadOffset;
    const uint8_t *mStart;
    const uint8_t *mOffset;
    const uint8_t *mEnd;
};

#define DATA_POOL_UINT32_BLOCK_SIZE 1024
//#define DATA_POOL_UINT32_BLOCK_SIZE 128

class DataPool
{
public:
    enum
    {
        kNumBuffers = 4,
        kBufferSize = DATA_POOL_UINT32_BLOCK_SIZE,
    };

    DataPool(void)
        : mBufferStart(0)
        , mBufferEnd(0)
    {
    }

    bool IsFull(void)
    {
        return (((mBufferEnd + 1) % kNumBuffers) == mBufferStart);
    }

    bool IsEmpty(void)
    {
        return (mBufferEnd == mBufferStart);
    }

    uint8_t GetNumValidBuffers(void)
    {
        return (mBufferEnd < mBufferStart) ? (mBufferEnd + kNumBuffers - mBufferStart): (mBufferEnd - mBufferStart);
    }

    otError In(uint32_t *aBuffer)
    {
        otError error = OT_ERROR_NONE;

        VerifyOrExit(!IsFull(), error = OT_ERROR_NO_BUFS);

        mBufferEnd = (mBufferEnd + 1) % kNumBuffers;
        memcpy(mBuffers[mBufferEnd], aBuffer, kBufferSize * sizeof(uint32_t));

exit:
        return error;
    }

    uint32_t *Out(void)
    {
        uint32_t *buffer;

        VerifyOrExit(!IsEmpty(), buffer = NULL);

        buffer = mBuffers[mBufferStart];
        mBufferStart = (mBufferStart + 1) % kNumBuffers;

exit:
        return buffer;
    }

private:
    uint8_t  mBufferStart;
    uint8_t  mBufferEnd;
    uint32_t mBuffers[kNumBuffers][kBufferSize];
};

class CliWav
{
public:
    CliWav(Interpreter &aInterpreter);

    otError Process(int argc, char *argv[]);

private:
    enum
    {
        kNumWavBuffers = 2,
        kWavBufferSize = DATA_POOL_UINT32_BLOCK_SIZE,
        kNumMicBuffers = 2,
        kMicBufferSize = 2 * DATA_POOL_UINT32_BLOCK_SIZE,
        kFlashWavStart = 0x60000, // Capacity: 512k Bytes
        kFlashWavEnd   = 0xE0000,
        //kReadInterval  = 2,
        kReadInterval  = 20,
        kSoundRetries  = 5,
    };


    static CliWav &GetOwner(OwnerLocator &aOwnerLocator);

    static const uint32_t *HandleSoundCallback(void *aContext);
    const uint32_t *HandleSoundCallback(void);

    static void HandleMicCallback(void *aContext, otMicEvent aEvent, uint16_t *aBuffer, uint16_t aLength);
    void HandleMicCallback(otMicEvent aEvent, uint16_t *aBuffer, uint16_t aLength);

    static void HandleTimer(Timer &aTimer);
    void HandleTimer(void);

    Interpreter &mInterpreter;
    Wav          mWav;

    DataPool mPool;

    TimerMilli mTimer;

    uint16_t mSoundRetries;
    uint32_t mMicStart;
    uint8_t  mBufferIndex;
    bool     mSoundRunning;
    bool     mMicRunning;
    uint32_t mWavOffset;
    uint32_t mTempBuffer[kWavBufferSize];
    uint32_t mWavBuffer[kNumWavBuffers][kWavBufferSize];
    uint16_t mMicBuffer[kNumMicBuffers][kMicBufferSize];
};

}  // namespace Cli
}  // namespace ot

#endif // CLI_WAV_HPP_
