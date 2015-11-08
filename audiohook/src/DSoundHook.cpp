#include "DSoundHook.h"

#include <dsound.h>

namespace HookStub
{
    typedef HRESULT(WINAPI *UnlockCallback)(IDirectSoundBuffer *_this, LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2);

    typedef HRESULT(WINAPI *DirectSoundCreateFn)(_In_opt_ LPCGUID pcGuidDevice, _Outptr_ LPDIRECTSOUND *ppDS, _Pre_null_ LPUNKNOWN pUnkOuter);

    COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE IDirectSoundBuffer_UnLock_(THIS_ IDirectSoundBuffer *_this, THIS_ _In_reads_bytes_(dwAudioBytes1) LPVOID pvAudioPtr1, DWORD dwAudioBytes1,
        _In_reads_bytes_opt_(dwAudioBytes2) LPVOID pvAudioPtr2, DWORD dwAudioBytes2)
    {
        DSoundHook& hook = DSoundHook::GetInstance();

        HRESULT hr = ((UnlockCallback)(hook.GetUnlockFunction()))(_this, pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
        if (S_OK == hr)
        {
            WAVEFORMATEX waveFormat;
            HRESULT fhr = _this->GetFormat(&waveFormat, sizeof waveFormat, NULL);

            if (S_OK == fhr)
            {
                hook.GetStream()->WriteRingBuffer(&waveFormat, (const int8_t *)pvAudioPtr1, dwAudioBytes1);
            }
        }

        return hr;
    }

#define LOCK_OFFSET 11
#define UNLOCK_OFFSET (LOCK_OFFSET + 8)
}

static void InitDefaultDesc(LPDSBUFFERDESC psc, PWAVEFORMATEX f)
{
    WAVEFORMATEX &waveInfo = *f;

    ::ZeroMemory(&waveInfo, sizeof waveInfo);

    waveInfo.wFormatTag = WAVE_FORMAT_PCM;
    waveInfo.nChannels = 2;
    waveInfo.nSamplesPerSec = 44100;
    waveInfo.wBitsPerSample = 16;

    waveInfo.nBlockAlign = 2 * 16 / 8;

    waveInfo.nAvgBytesPerSec = waveInfo.nSamplesPerSec * waveInfo.nBlockAlign;
    waveInfo.cbSize = 0;

    DSBUFFERDESC &sc = *psc;
    ::ZeroMemory(&sc, sizeof sc);
    sc.dwSize = sizeof(DSBUFFERDESC);
    sc.dwBufferBytes = 4096 * waveInfo.nBlockAlign;
    sc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    sc.lpwfxFormat = f;
}

DSoundHook& DSoundHook::GetInstance()
{
    static DSoundHook hook;
    return hook;
}

DSoundHook::DSoundHook()
{
    _dsound = NULL;
    _stream = NULL;
}

DSoundHook::~DSoundHook()
{
    Unhook();
}

bool DSoundHook::Hook(AudioOutputStream *outputStream)
{
    _dsound = ::GetModuleHandle(L"dsound");

    if (!_dsound)
        return false;

    bool success = false;

    HookStub::DirectSoundCreateFn createFn = (HookStub::DirectSoundCreateFn)::GetProcAddress(_dsound, "DirectSoundCreate");
    if (createFn)
    {
        LPDIRECTSOUND lpDs = NULL;
        if (S_OK == createFn(NULL, &lpDs, NULL))
        {
            WAVEFORMATEX waveInfo;
            DSBUFFERDESC sc;

            InitDefaultDesc(&sc, &waveInfo);

            LPDIRECTSOUNDBUFFER lpBuffer;
            lpDs->CreateSoundBuffer(&sc, &lpBuffer, NULL);

            if (lpBuffer)
            {
                PDWORD pVTable = *(PDWORD *)(lpBuffer);
                DWORD unlockAddr = pVTable[UNLOCK_OFFSET];

                /* inject our proc */
                DWORD oldProtectFlag = 0;
                if (::VirtualProtect((PVOID)pVTable[LOCK_OFFSET], (UNLOCK_OFFSET - LOCK_OFFSET) * 4, PAGE_EXECUTE_READWRITE, &oldProtectFlag))
                {
                    _pVt = pVTable;
                    _unlockFuncAddr = unlockAddr;

                    pVTable[UNLOCK_OFFSET] = (DWORD)HookStub::IDirectSoundBuffer_UnLock_;

                    DWORD noused = 0;
                    ::VirtualProtect((PVOID)pVTable[LOCK_OFFSET], (UNLOCK_OFFSET - LOCK_OFFSET) * 4, oldProtectFlag, &noused);

                    _stream = outputStream;
                    success = true;
                }

                lpBuffer->Release();
            }

            lpDs->Release();
        }
    }

    if (!success)
        Unhook();

    return success;
}

void DSoundHook::Unhook()
{
    if (_dsound)
    {
        HMODULE hDSound = ::GetModuleHandle(L"dsound");
        if (_dsound == hDSound && _pVt && _unlockFuncAddr)
        {
            DWORD oldProtectFlag = 0;
            if (::VirtualProtect((PVOID)_pVt[LOCK_OFFSET], (UNLOCK_OFFSET - LOCK_OFFSET) * 4, PAGE_EXECUTE_READWRITE, &oldProtectFlag))
            {
                _pVt[UNLOCK_OFFSET] = _unlockFuncAddr;

                DWORD noused = 0;
                ::VirtualProtect((PVOID)_pVt[LOCK_OFFSET], (UNLOCK_OFFSET - LOCK_OFFSET) * 4, oldProtectFlag, &noused);
            }
        }
    }

    _dsound = NULL;
    _pVt = NULL;
    _unlockFuncAddr = 0;
    _stream = NULL;
}