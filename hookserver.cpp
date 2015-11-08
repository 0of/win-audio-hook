#include "windows.h"
#include "TlHelp32.h"
#include <iostream>

#include "hookserver.h"

typedef HMODULE(WINAPI *FnLoadLibraryW)(_In_ LPCWSTR);

static HINSTANCE DLLInstance = NULL;

typedef struct tagInterceptor
{
    WCHAR lpInterceptorPath[MAX_PATH];
    FnLoadLibraryW pLoadLibrary;
} Interceptor, *PInterceptor;

static BOOL WINAPI WriteDataToRemoteProcess(HANDLE hProcess, CONST PInterceptor pInterceptor, PVOID *ppArgs)
{
    SIZE_T writeCount = 0;

    /* pass arguments to remote process */
    PVOID pThreadArgs = ::VirtualAllocEx(hProcess, NULL, sizeof WCHAR * MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!::WriteProcessMemory(hProcess, pThreadArgs, pInterceptor, sizeof WCHAR * MAX_PATH, &writeCount))
    {
        std::cout << "write args failed";
    }

    if (pThreadArgs)
    {
        *ppArgs = pThreadArgs;
        return TRUE;
    }

    return FALSE;
}

static BOOL WINAPI FindProcAddrByName(LPCWSTR lpName, DWORD dwPID, PMODULEENTRY32 pme)
{
    BOOL bFoundYet = FALSE;

    /* search kernel32 module */
    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID);

    /* fatal error */
    if (INVALID_HANDLE_VALUE == hModuleSnap)
        return bFoundYet;

    ::ZeroMemory(pme, sizeof *pme);
    pme->dwSize = sizeof(MODULEENTRY32);

    if (!Module32First(hModuleSnap, pme))
        goto ModuleCleanup;

    do
    {
        if (0 == ::lstrcmpW(lpName, pme->szModule))
        {
            bFoundYet = TRUE;
            break;
        }

    } while (Module32Next(hModuleSnap, pme));

ModuleCleanup:
    ::CloseHandle(hModuleSnap);

    return bFoundYet;
}

static BOOL WINAPI GetLoadLibraryProcAddr(DWORD dwPID, SIZE_T dwProcOffset, _Outptr_ PInterceptor pInterceptor)
{
    MODULEENTRY32 me;

    if (FindProcAddrByName(L"kernel32.dll", dwPID, &me))
    {
        pInterceptor->pLoadLibrary = (FnLoadLibraryW)(me.modBaseAddr + dwProcOffset);
        return TRUE;
    }

    return FALSE;
}

class HookException : public Exception
{
private:
    std::string reason;

public:
    template<typename T>
    explicit HookException(T && reason)
        : reason(std::forward<T>(reason))
    {}

    virtual ~HookException() {}
    virtual std::string What() const { return reason; }
    virtual Exception *Clone() const { return new HookException(reason); }
};

HookedProcess HookTask::Run()
{
    /* init interceptor */
    Interceptor interc;
    ::ZeroMemory(&interc, sizeof interc);
    ::lstrcpyW(interc.lpInterceptorPath, _dll_path.c_str());

    HANDLE hEachProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, _pid);

    std::string failedReason;

    if (hEachProcess && GetLoadLibraryProcAddr(_pid, _proc_offset, &interc))
    {
        LPVOID lpArgs = NULL;

        if (WriteDataToRemoteProcess(hEachProcess, &interc, &lpArgs))
        {
            DWORD dwThreadID = 0;
            HANDLE hThread = ::CreateRemoteThread(hEachProcess, NULL, 4096, (LPTHREAD_START_ROUTINE)interc.pLoadLibrary, lpArgs, 0, &dwThreadID);

            /* create failed */
            if (!hThread)
            {
                failedReason = "create remote thread failed";

                /* do some clean up for remote process */
                std::cout << failedReason;
            }
            else
            {
                std::cout << "injection successfully";
                ::WaitForSingleObject(hThread, INFINITE);
            }

            ::VirtualFree(lpArgs, sizeof WCHAR * MAX_PATH, MEM_RELEASE);
        }
    }
    else
    {
        /* ignore or log */
        failedReason = "open process failed or invalid process";
    }

    if (hEachProcess)
        ::CloseHandle(hEachProcess);

    if (!failedReason.empty())
        throw HookException(failedReason);

    /* get process name */
    return HookedProcess{ _process_name, _pid };
}

void RemoveHookTask::Run()
{
    std::wstring moduleName;

    /* get trimed module name */
    auto foundPos = _dll_path.rfind(L'\\');
    if (foundPos != std::wstring::npos)
    {
        moduleName = _dll_path.substr(foundPos + 1);
    }

    /* invalid path */
    if (moduleName.empty())
        return;

    /* get offset of free library */
    HINSTANCE hKernel32 = ::GetModuleHandle(L"kernel32");
    PBYTE pFn = (PBYTE)::GetProcAddress(hKernel32, "FreeLibrary");
    SIZE_T offset = (pFn - (PBYTE)hKernel32);

    MODULEENTRY32 me;

    for (auto each : _pids)
    {
        HANDLE hEachProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, each);
        if (hEachProcess)
        {
            LPTHREAD_START_ROUTINE lpProc = NULL;
            if (FindProcAddrByName(L"kernel32.dll", each, &me))
            {
                lpProc = (LPTHREAD_START_ROUTINE)(me.modBaseAddr + offset);

                if (FindProcAddrByName(moduleName.c_str(), each, &me))
                {
                    DWORD dwThreadID = 0;
                    HANDLE hThread = ::CreateRemoteThread(hEachProcess, NULL, 4096, lpProc, me.hModule, 0, &dwThreadID);

                    if (hThread)
                    {
                        ::WaitForSingleObject(hThread, INFINITE);

                        std::cout << "remove OK, PID:" << each;
                    }
                }
            }

            ::CloseHandle(hEachProcess);
        }
    }
}
