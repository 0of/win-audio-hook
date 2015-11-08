#include "WaveOutHook.h"
#include "Mmreg.h"

namespace HookStub
{
    MMRESULT
    WINAPI
    waveOutOpenStub(
        _Out_opt_ LPHWAVEOUT phwo,
        _In_ UINT uDeviceID,
        _In_ LPCWAVEFORMATEX pwfx,
        _In_opt_ DWORD_PTR dwCallback,
        _In_opt_ DWORD_PTR dwInstance,
        _In_ DWORD fdwOpen
        )
    {
        WaveOutHook& hook = WaveOutHook::GetInstance();
        PUCHAR pTargetProcAddr = hook.GetAddress("waveOutOpen");

        if (!pTargetProcAddr)
            return WAVERR_BADFORMAT;

        pTargetProcAddr += 5;

        bool success = hook.BeginAddHookHandle();

        MMRESULT realResult = 0;
        __asm
        {
            ;; save jump address
            mov eax, pTargetProcAddr

            ;; push the arguments to fulfill the calling
            push fdwOpen
            push dwInstance
            push dwCallback
            push pwfx
            push uDeviceID
            push phwo

            ;; push return address
            push Return

            ;; first 5 bytes instructions
            mov edi, edi
            push ebp
            mov ebp, esp

            ;; jump back to wimmm module address
            jmp eax

        Return :
            ;; get return value
            mov realResult, eax
        }

        if (success && !(fdwOpen & WAVE_FORMAT_QUERY) && realResult == MMSYSERR_NOERROR)
        {
            /* insert to global table */
            hook.EndAddHookHandle(*phwo, pwfx);
        }

        return realResult;
    }

    MMRESULT
    WINAPI
    waveOutCloseStub(
        _In_ HWAVEOUT hwo
        )
    {
        WaveOutHook& hook = WaveOutHook::GetInstance();
        PUCHAR pTargetProcAddr = hook.GetAddress("waveOutClose");

        if (!pTargetProcAddr)
            return MMSYSERR_INVALHANDLE;

        pTargetProcAddr += 5;

        MMRESULT realResult = 0;

        __asm
        {
            ;; save jump address
            mov eax, pTargetProcAddr

            ;; push the arguments to fulfill the calling
            push hwo

            ;; push return address
            push Return

            ;; first 5 bytes instructions
            mov edi, edi
            push ebp
            mov ebp, esp

            ;; jump back to wimmm module address
            jmp eax

        Return :
            ;; get return value
            mov realResult, eax
        }

        hook.RemoveHandle(hwo);

        return realResult;
    }

    MMRESULT
    WINAPI
    waveOutWriteStub(
        _In_ HWAVEOUT hwo,
        _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh,
        _In_ UINT cbwh
        )
    {
#if LOGGER
            if (_logger)
                *_logger << (ULONG)(hwo) << std::endl;
#endif

        WAVEFORMATEX format;
        ::ZeroMemory(&format, sizeof format);

        WaveOutHook& hook = WaveOutHook::GetInstance();
        bool success = hook.GetFormat(hwo, &format);

        PUCHAR pTargetProcAddr = hook.GetAddress("waveOutWrite");

        if (!pTargetProcAddr)
            return MMSYSERR_INVALHANDLE;

        pTargetProcAddr += 5;

        MMRESULT realResult = 0;

        __asm
        {
            ;; save jump address
            mov eax, pTargetProcAddr

            ;; push the arguments to fulfill the calling
            push cbwh
            push pwh
            push hwo

            ;; push return address
            push Return

            ;; first 5 bytes instructions
            mov edi, edi
            push ebp
            mov ebp, esp

            ;; jump back to wimmm module address
            jmp eax

        Return:
            ;; get return value
            mov realResult, eax
        }

        if (realResult == MMSYSERR_NOERROR && success)
        {
            hook.GetStream()->WriteRingBuffer(&format, reinterpret_cast<const int8_t *>(pwh->lpData), pwh->dwBufferLength);
            
            /* release */
            hook.ReleaseContext(hwo);
        }

        return realResult;
    }
}

namespace HookImpl
{
    template<typename T>
    class AutoLock
    {
    private:
        T *m_locker;

    public:
        AutoLock(T *locker)
            : m_locker(locker)
        {
            m_locker->Enter();
        }

        ~AutoLock()
        {
            m_locker->Leave();
        }
    };

    const UCHAR HookProcFirst5Bytes[] =
    {
        /* mov  edi, edi */
        0x8b, 0xff,
        /* push ebp */
        0x55,
        /* mov  ebp, esp */
        0x8b, 0xec
    };

    static bool Inject(PUCHAR pTargetProcAddr, LONG lAddr)
    {
        if (!pTargetProcAddr)
            return false;

        PVOID pJumpOffset = (((PUCHAR)pTargetProcAddr) + 5);
        UCHAR pJumpIns[] = { 0xe9, 0, 0, 0, 0 };

        DWORD dwOldProtect = 0;
        if (::VirtualProtect((PVOID)pTargetProcAddr, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        {
            LONG lRelativeAddr = (LONG)(lAddr)-(LONG)pJumpOffset;

            ::CopyMemory(pJumpIns + 1, &lRelativeAddr, 4);

            /* all ok, then write first 5 bytes of target procedure */
            ::CopyMemory(pTargetProcAddr, pJumpIns, 5);

            DWORD unused = 0;
            ::VirtualProtect((PVOID)pTargetProcAddr, 5, dwOldProtect, &unused);
            
            return true;
        }

        return false;
    }

    static bool Remove(PUCHAR pTargetProcAddr)
    {
        DWORD dwOldProtect = 0;
        if (::VirtualProtect((PVOID)pTargetProcAddr, 5, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        {
            ::CopyMemory(pTargetProcAddr, HookProcFirst5Bytes, 5);

            DWORD unused = 0;
            ::VirtualProtect((PVOID)pTargetProcAddr, 5, dwOldProtect, &unused);

            return true;
        }

        return false;
    }
}

WaveOutHook::CriticalSection::CriticalSection()
: m_criticalSection()
{
    ::InitializeCriticalSection(&m_criticalSection);
}

WaveOutHook::CriticalSection::~CriticalSection()
{
    ::DeleteCriticalSection(&m_criticalSection);
}

void WaveOutHook::CriticalSection::Enter()
{
    ::EnterCriticalSection(&m_criticalSection);
}

void WaveOutHook::CriticalSection::Leave()
{
    ::LeaveCriticalSection(&m_criticalSection);
}

WaveOutHook& WaveOutHook::GetInstance()
{
    static WaveOutHook hook;
    return hook;
}

WaveOutHook::WaveOutHook()
{
    _winmm = NULL;
}

WaveOutHook::~WaveOutHook()
{
    Unhook();
}

void WaveOutHook::EndAddHookHandle(HWAVEOUT h, LPCWAVEFORMATEX f)
{
    {
        HookImpl::AutoLock<decltype(_openCs)> unusedOpen(&_openCs);
        _openContexts.erase(::GetCurrentThreadId());
    }

    {
        HookImpl::AutoLock<decltype(_cs)> unused(&_cs);
        _handles[h] = WaveDevInfo{ *f };
    }
}

bool WaveOutHook::BeginAddHookHandle()
{
    DWORD context = ::GetCurrentThreadId();
    HookImpl::AutoLock<decltype(_openCs)> unusedOpen(&_openCs);
    if (_openContexts.find(context) != _openContexts.end())
        return false;

    _openContexts.insert(context);
    return true;
}

void WaveOutHook::RemoveHandle(HWAVEOUT h)
{
    HookImpl::AutoLock<decltype(_cs)> unused(&_cs);
    _handles.erase(_handles.find(h));
}

void WaveOutHook::Clear()
{
    HookImpl::AutoLock<decltype(_cs)> unused(&_cs);
    _handles.clear();
}

bool WaveOutHook::GetFormat(HWAVEOUT h, PWAVEFORMATEX format)
{
    HookImpl::AutoLock<decltype(_cs)> unused(&_cs);

    auto found = _handles.find(h);
    if (found != _handles.end())
    {
        auto& contexts = found->second.contexts;

        DWORD context = ::GetCurrentThreadId();
        auto contextFound = contexts.find(context);

        /* recursive calling */
        if (contextFound != contexts.end())
            return false;

        contexts.insert(context);

        ::CopyMemory(format, &found->second.format, sizeof *format);

        return true;
    }

    return false;
}

void WaveOutHook::ReleaseContext(HWAVEOUT h)
{
    HookImpl::AutoLock<decltype(_cs)> unused(&_cs);

    auto found = _handles.find(h);
    if (found != _handles.end())
    {
        auto& contexts = found->second.contexts;
        contexts.erase(contexts.find(::GetCurrentThreadId()));
    }
}

PUCHAR WaveOutHook::GetAddress(LPCSTR symbol)
{
    if (_winmm && symbol)
    {
        std::string symbolString = symbol;

        auto found = _addrCache.find(symbolString);
        if (found != _addrCache.end())
            return found->second;
        else
            return (PUCHAR)::GetProcAddress(_winmm, symbol);
    }

    return NULL;
}

bool WaveOutHook::Hook(AudioOutputStream *outputStream)
{
    if (_winmm)
        return false;

    _winmm = ::GetModuleHandle(L"winmm");
    if (!_winmm)
        /* no winmm module */
        return false;

    _stream = outputStream;

    if (!HookImpl::Inject(GetAddress("waveOutOpen"), (LONG)&HookStub::waveOutOpenStub))
        goto Cleanup;

    if (!HookImpl::Inject(GetAddress("waveOutWrite"), (LONG)&HookStub::waveOutWriteStub))
        goto Cleanup;

    if (!HookImpl::Inject(GetAddress("waveOutClose"), (LONG)&HookStub::waveOutCloseStub))
        goto Cleanup;
    
    /* all injection successfully */
    return true;

Cleanup:
    Unhook();
    return false;
}

void WaveOutHook::Unhook()
{
    if (!_winmm)
        return;

    HookImpl::Remove(GetAddress("waveOutOpen"));
    HookImpl::Remove(GetAddress("waveOutWrite"));
    HookImpl::Remove(GetAddress("waveOutClose"));

    Clear();
    _stream = NULL;
    _winmm = NULL;
}