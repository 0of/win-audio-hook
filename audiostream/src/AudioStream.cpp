#include "AudioStream.h"

#include <limits>

void DbgStrOut(const CHAR *fmt, ...)
{
    CHAR szOutStr[512];

    va_list ap;
    va_start(ap, fmt);
    vsprintf(szOutStr, fmt, ap);
    va_end(ap);

    OutputDebugStringA(szOutStr);
}

#define DEBUG_LOG(fmt, ...) DbgStrOut(fmt,  __VA_ARGS__)

#define WAVE_FORMAT_IEEE_FLOAT 3

typedef struct tagSTREAMHEAD
{
    DWORD occupied;
    DWORD readseek;
    DWORD blen;
    WAVEFORMATEX info;
    WORD reseverd;
} STREAMHEAD, *PSTREAMHEAD;

static const std::int32_t RingBufferBlockCount = 16;
static const std::int32_t RingBufferBlockSize = 8196;

class AutoOccupied
{
private:
    PSTREAMHEAD _pHead;

public:
    AutoOccupied(PSTREAMHEAD pHead, bool *owned)
        : _pHead(NULL)
    {
        bool hasOwned = InterlockedExchange(&pHead->occupied, 1) == 0;
        if (hasOwned)
            _pHead = pHead;

        *owned = hasOwned;
    }

    ~AutoOccupied()
    {
        if (_pHead)
        {
            InterlockedExchange(&_pHead->occupied, 0);
        }
    }
};

AudioOutputStream::AudioOutputStream()
    : _buffer()
    , _writeBlockIndex(0)
{
}

AudioOutputStream::~AudioOutputStream()
{

}

void AudioOutputStream::WriteRingBuffer(CONST PWAVEFORMATEX pInfo, const std::int8_t *const pData, const std::uint32_t len)
{
    if (0 == pInfo->nBlockAlign || 0 == pInfo->nChannels || 0 == pInfo->wBitsPerSample)
        return;

    if (0 != len % pInfo->nBlockAlign)
        return;

    if (_writeBlockIndex >= RingBufferBlockCount)
        return;

    if (_buffer.IsOpen())
    {
        std::uint32_t index = _writeBlockIndex;
        std::uint32_t bufferOffset = 0;

        /* traversal the ring */
        while (true)
        {
            /* check whether read buffer has unfilled space */
            if (bufferOffset >= len)
                break;

            PSTREAMHEAD pHead = reinterpret_cast<PSTREAMHEAD>(_buffer.GetRawPtr(0)) + index;

            /* check ring buffer is full */
            DWORD canWriteLength = InterlockedCompareExchange(&pHead->blen, 0, 0);

            /* current block has not been processed, drop current buffer */
            if (canWriteLength)
                break;

            std::uint32_t aboutToWriteLength = 0;
            {
                /* occupy that buffer */
                bool hasOccupied = false;
                AutoOccupied occupied(pHead, &hasOccupied);

                /* rarely happened */
                if (!hasOccupied)
                    break;

                /* locate specific buffer block */
                std::int8_t *blockStart = reinterpret_cast<std::int8_t *>(_buffer.GetRawPtr(0) + ((sizeof STREAMHEAD) * RingBufferBlockCount) + (index * RingBufferBlockSize));

                /* calculate the nearest buffer boundary */
                std::uint32_t boundary = (RingBufferBlockSize / pInfo->nBlockAlign) * pInfo->nBlockAlign;

                aboutToWriteLength = min(boundary, len - bufferOffset);

                /* rarely happened */
                if (!aboutToWriteLength)
                    break;

                ::CopyMemory(blockStart, pData + bufferOffset, aboutToWriteLength);
                ::CopyMemory(&pHead->info, pInfo, sizeof *pInfo);

                DEBUG_LOG("Write index : %d count in bytes:%d\n", index, aboutToWriteLength);

                pHead->readseek = 0;
            }

            /* update head length */
            InterlockedExchange(&pHead->blen, aboutToWriteLength);

            /* update index */
            ++index; 

            index = index % RingBufferBlockCount;

            bufferOffset += aboutToWriteLength;

            /* write finished */
            if (bufferOffset >= len)
                break;
        }

        _writeBlockIndex = index;
    }
}


bool AudioOutputStream::Open(const std::wstring& devName)
{
    if (_buffer.IsOpen())
        return true;

    if (_buffer.Open(devName.c_str(), ::GetCurrentProcessId(), 1024 * 512, 0))
    {
        ::ZeroMemory(_buffer.GetRawPtr(0), 1024 * 512);
        return true;
    }

    return false;
}

bool AudioInputStream::Open(const std::wstring& devName, const std::uint32_t pid)
{
    return _buffer.IsOpen() || _buffer.Open(devName.c_str(), pid, 0, 1);
}

struct ReadWaveBufferBlock
{
    float *data;
    std::uint32_t samplelen;
    std::uint32_t channels;
};

struct InputWaveBlock
{
    std::int8_t *data;
    LPWAVEFORMATEX format;
};

#define APPEND_PROC(bitsCount) \
for (auto i = 0; i != readBuffer->samplelen; ++i) \
{ \
for (auto channel = 0; channel != sampleChannel; ++channel) \
{ \
    bufferStart[channel] += (reinterpret_cast<std::int##bitsCount##_t *>(readStart)[channel] / static_cast<std::float_t>(std::numeric_limits<std::int##bitsCount##_t>::max())); \
    } \
        \
        bufferStart += readBuffer->channels; \
        readStart += inputWaveBlock->format->nBlockAlign; \
    }\
    appendSampleCount += readBuffer->samplelen;

/* no resample */
static std::uint32_t _AppendBuffer(ReadWaveBufferBlock *readBuffer, InputWaveBlock *inputWaveBlock)
{
    std::uint32_t appendSampleCount = 0;

    std::uint32_t sampleChannel = min(inputWaveBlock->format->nChannels, readBuffer->channels);
    if (!sampleChannel)
        return appendSampleCount;

    std::uint32_t bitsPersample = inputWaveBlock->format->wBitsPerSample;

    std::int8_t *readStart = inputWaveBlock->data;
    auto bufferStart = readBuffer->data;

#pragma push_macro("max")
#undef max
    if (8 == bitsPersample)
    {
        APPEND_PROC(8)
    }
    else if (16 == bitsPersample)
    {
        APPEND_PROC(16)
    }
    else if (32 == bitsPersample)
    {
        if (inputWaveBlock->format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        {
            /* float */
            for (auto i = 0; i != readBuffer->samplelen; ++i)
            {
                for (auto channel = 0; channel != sampleChannel; ++channel)
                {
                    bufferStart[channel] += (reinterpret_cast<std::float_t *>(readStart)[channel]);
                }

                bufferStart += readBuffer->channels;
                readStart += inputWaveBlock->format->nBlockAlign;
            }
            appendSampleCount += readBuffer->samplelen;
        }
        else
        {
            APPEND_PROC(32)
        }
    }
#pragma pop_macro("max")
    else
    {
        return appendSampleCount;
    }

    return appendSampleCount;
}

/* may result integer overflow */
uint32_t AudioInputStream::ReadRingBufferAndAppend(std::float_t *buffer, std::uint32_t len, CONST LPWAVEFORMATEX info)
{
    uint32_t readLength = 0;

    if (_readBlockIndex >= RingBufferBlockCount)
        /* broken */
        return readLength;

    if (_buffer.IsOpen())
    {
        std::uint32_t index = _readBlockIndex;
        std::uint32_t bufferOffset = 0;

        ReadWaveBufferBlock bufferBlock{ nullptr, 0, info->nChannels };
        InputWaveBlock inputBlock;

        /* traversal the ring */
        while (true)
        {
            /* check whether read buffer has unfilled space */
            if (bufferOffset >= len)
                break;

            PSTREAMHEAD pHead = reinterpret_cast<PSTREAMHEAD>(_buffer.GetRawPtr(0)) + index;
        
            /* check ring buffer is empty */
            DWORD canReadLength = InterlockedCompareExchange(&pHead->blen, 0, 0);

            /* is empty */
            if (!canReadLength)
                break;
  
            /* occupy that buffer */
            bool hasOccupied = false;
            AutoOccupied occupied(pHead, &hasOccupied);

            /* rarely happened */
            if (!hasOccupied)
                /* just break, wait for next time read */
                break;

            /* validate buffer head */
            if (canReadLength > pHead->readseek && 
                0 != pHead->info.wBitsPerSample && 
                0 != pHead->info.nBlockAlign)
            {
                /* about to read sample count */
                std::uint32_t aboutToReadLength = min((canReadLength - pHead->readseek) / (pHead->info.nBlockAlign), (len - bufferOffset) / info->nChannels);
                if (aboutToReadLength)
                {
                    /* start to read */
                    bufferBlock.data = buffer + bufferOffset;
                    bufferBlock.samplelen = aboutToReadLength;

                    inputBlock.data = reinterpret_cast<int8_t *>(_buffer.GetRawPtr(0) + ((sizeof STREAMHEAD) * RingBufferBlockCount) + (index * RingBufferBlockSize)) 
                        + pHead->readseek;
                    DEBUG_LOG("About to Read data addr : %d\n", inputBlock.data);

                    inputBlock.format = &pHead->info;

                    std::uint32_t appendedSampleCount = _AppendBuffer(&bufferBlock, &inputBlock);

                    DEBUG_LOG("Read index : %d count in bytes: %d\n", index, appendedSampleCount * pHead->info.nBlockAlign);

                    /* read sample and update offset */
                    if (appendedSampleCount)
                    {
                        /* update offset */
                        bufferOffset += appendedSampleCount * info->nChannels;
                        pHead->readseek += appendedSampleCount * pHead->info.nBlockAlign;

                        readLength += appendedSampleCount;

                        /* not read all of data from current block */
                        if (pHead->readseek < pHead->blen)
                            /* not enough buffer */
                            break;
                    }

                    /* if no data read, this block may not be supported, so jump to next one */
                    else {}
                }
            }

            /* remove that buffer */
            InterlockedExchange(&pHead->blen, 0);

            /* go next head */
            ++index; index = index % RingBufferBlockCount; continue;
        }

        /* update index */
        _readBlockIndex = index;
    }

    return readLength;
}

AudioInputStream::AudioInputStream()
    : _buffer()
    , _readBlockIndex(0)
{
}

AudioInputStream::~AudioInputStream()
{

}

