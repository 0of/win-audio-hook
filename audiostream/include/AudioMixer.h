#pragma once

#include <set>
#include <string>

#include "AudioStream.h"

class WavOutFile;

class AUDIOSTREAM_EXPORT AudioMixer
{
private:
    std::set<AudioInputStream *> _streams;

    WAVEFORMATEX _waveInfo;
    WavOutFile *_waveFile;

    std::float_t *_buffer;
    std::float_t *_writeBuffer;

    /* 20ms buffer length */
    /* sizeof int * 8 * sample per sec / 1000 * 20 * channels(2) */
    static const int BufferLength = 4 * ((44100 / 100) * 2) * 2;

public:
    AudioMixer();
    ~AudioMixer();

public:
    bool Open(PWAVEFORMATEX pWaveInfo, const std::wstring& path);
    void Close();

    void AddStream(AudioInputStream *stream);

    /* write data to Wave file */
    void Update();
};

