#pragma once

#include <Windows.h>
#include <map>
#include <set>

#include "AudioStream.h"

class WaveOutHook
{
private:
    class CriticalSection
    {
        CRITICAL_SECTION m_criticalSection;

    public:
        CriticalSection();
        ~CriticalSection();

    public:
        void Enter();
        void Leave();
    };

private:
    HMODULE _winmm;

    struct WaveDevInfo
    {
        WAVEFORMATEX format;
        std::set<DWORD> contexts;
    };

    std::set<DWORD> _openContexts;
    CriticalSection _openCs;

    std::map<HWAVEOUT, WaveDevInfo> _handles;
    std::map<std::string, PUCHAR> _addrCache;

    AudioOutputStream *_stream;

    CriticalSection _cs;

public:
    static WaveOutHook& GetInstance();

public:
    WaveOutHook();
    ~WaveOutHook();

public:
    bool Hook(AudioOutputStream *outputStream);
    void Unhook();

public:
    bool BeginAddHookHandle();
    void EndAddHookHandle(HWAVEOUT, LPCWAVEFORMATEX);

    void RemoveHandle(HWAVEOUT);

    void Clear();

    /* implicitly acquire context */
    bool GetFormat(HWAVEOUT, PWAVEFORMATEX format);
    void ReleaseContext(HWAVEOUT);

public:
    PUCHAR GetAddress(LPCSTR symbol);
    inline AudioOutputStream *GetStream() const { return _stream; }
};

