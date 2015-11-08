#pragma once

#include <Windows.h>
#include <map>

#include "AudioStream.h"

class DSoundHook
{
private:
    HMODULE _dsound;

    DWORD _unlockFuncAddr;
    PDWORD _pVt;

    AudioOutputStream *_stream;

public:
    static DSoundHook& GetInstance();

public:
    DSoundHook();
    ~DSoundHook();

public:
    bool Hook(AudioOutputStream *outputStream);
    void Unhook();

public:
    DWORD GetUnlockFunction() const { return _unlockFuncAddr; }
    inline AudioOutputStream *GetStream() const { return _stream; }
};

