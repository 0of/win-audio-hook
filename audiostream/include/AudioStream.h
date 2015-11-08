#pragma once

#include "AudioStreamGlobals.h"

#include <Windows.h>
#include <mmeapi.h>

#include <string>
#include <cstdint>
#include <vector>

#include "SharedMemory.h"

class AUDIOSTREAM_EXPORT AudioOutputStream
{
private:
    CSharedMemory _buffer;
    std::uint32_t _writeBlockIndex;

public:
    AudioOutputStream();
    virtual ~AudioOutputStream();

public:
    bool Open(const std::wstring& devName);

public:
    /* thread safe */
    /* write to ring buffer and check arugments */
    void WriteRingBuffer(CONST PWAVEFORMATEX pInfo, const std::int8_t *const pData, const std::uint32_t len);
};

class AUDIOSTREAM_EXPORT AudioInputStream
{
private:
    std::uint32_t _readBlockIndex;
    CSharedMemory _buffer;

public:
    AudioInputStream();
    ~AudioInputStream();

public:
    bool Open(const std::wstring& devName, const std::uint32_t pid);

    /* read and append its value to buffer */
    /* return sample count */
    uint32_t ReadRingBufferAndAppend(std::float_t *buffer, std::uint32_t len, CONST LPWAVEFORMATEX info);
};

