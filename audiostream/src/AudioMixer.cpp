#include "AudioMixer.h"

#include <limits>
#pragma warning(disable: 4723) // divide 0
#pragma warning(disable: 4244) // possible loss of data

#include "WavFile.h"

AudioMixer::AudioMixer()
: _streams()
, _waveInfo()
, _waveFile(NULL)
, _buffer(NULL)
, _writeBuffer(NULL)
{

}

AudioMixer::~AudioMixer()
{
    Close();
}

void AudioMixer::AddStream(AudioInputStream *stream)
{
    _streams.insert(_streams.end(), stream);
}

bool AudioMixer::Open(PWAVEFORMATEX pWaveInfo, const std::wstring& path)
{
    if (16 != pWaveInfo->wBitsPerSample)
        return false;

    ::CopyMemory(&_waveInfo, pWaveInfo, sizeof *pWaveInfo);

    /* create buffer */
    _buffer = (std::float_t *)::malloc(BufferLength);
    _writeBuffer = (std::float_t *)::malloc(BufferLength);

    if (_buffer && _writeBuffer)
    {
        _waveFile = WavOutFile::Create(path.c_str(), pWaveInfo->nSamplesPerSec, pWaveInfo->wBitsPerSample, pWaveInfo->nChannels);
        return NULL != _waveFile;
    }

    ::free(_buffer);
    ::free(_writeBuffer);

    return false;
}

void AudioMixer::Close()
{
    ::free(_buffer);
    ::free(_writeBuffer);

    if (_waveFile)
    {
        delete _waveFile;
        _waveFile = NULL;
    }
    
    for (auto each : _streams)
    {
        delete each;
    }

    _streams.clear();
}

void AudioMixer::Update()
{
    DWORD now = ::GetTickCount();

    while (true)
    {
        std::int32_t chanelCounter = 0;
        std::uint32_t sampleCount = 0;

        /* clr */
        ::memset(_buffer, 0, BufferLength);

        for (auto each : _streams)
        {
            auto appendSampleCount = each->ReadRingBufferAndAppend(_buffer, BufferLength / 4 /* int length */, &_waveInfo);
            if (0 != appendSampleCount)
            {
                ++chanelCounter;
                sampleCount = max(sampleCount, appendSampleCount);
            }
        }

        if (chanelCounter)
        {
            auto writeSampleCount = min(sampleCount * 2, BufferLength / 4);

            for (auto i = 0; i != writeSampleCount; ++i)
                _writeBuffer[i] = (_buffer[i] / chanelCounter);

            _waveFile->write(_writeBuffer, writeSampleCount);
        }
        else
        {
            /* no data to write */
            break;
        }
    }
}