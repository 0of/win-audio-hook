#pragma once

#include <string>
#include <vector>

#include "Promise.h"

struct HookedProcess
{
    std::wstring name;
    std::uint32_t pid;
};

class HookTask : public Task<HookedProcess>
{
private:
    std::wstring _process_name;
    std::wstring _dll_path;
    std::uint32_t _pid;
    std::size_t _proc_offset;

public:
    HookTask(const std::wstring& name, const std::wstring& path, std::uint32_t pid, std::size_t offset)
        : _process_name(name)
        , _dll_path(path)
        , _pid(pid)
        , _proc_offset(offset)
    {}

    virtual ~HookTask() {}
    virtual HookedProcess Run();
};

class RemoveHookTask : public Task<void>
{
private:
    std::vector<std::uint32_t> _pids;
    std::wstring _dll_path;

public:
    template<typename T>
    RemoveHookTask(T&& pids, const std::wstring& path, std::uint32_t pid)
        : _pids(std::forward<T>(pids))
        , _dll_path(path)
    {}

    virtual ~RemoveHookTask() {}
    virtual void Run();
};