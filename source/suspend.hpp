#pragma once

#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winternl.h>

class SuspendMutex {
public:
    SuspendMutex() { 
        suspended.reserve(0x400);
    }

    SuspendMutex(const SuspendMutex&) = delete;
    void operator=(const SuspendMutex&) = delete;

    void lock() {
        mutex.lock();
        suspend();
    }

    void unlock() {
        mutex.unlock();
        resume();
    }

    [[nodiscard]] bool try_lock() noexcept {
        if (!mutex.try_lock())
            return false;
        suspend();
        return true;
    }

private:
    using NtGetNextThreadFn = NTSTATUS(*)(HANDLE ProcessHandle, HANDLE ThreadHandle,
    	ACCESS_MASK DesiredAccess, ULONG HandleAttributes, ULONG Flags, PHANDLE NewThreadHandle);

    void suspend() {
        suspended.reserve(0x400);
        NtGetNextThreadFn NtGetNextThread = getNtGetNextThread();
        HANDLE thisProcess = GetCurrentProcess();
        DWORD thisThread = GetCurrentThreadId();
        DWORD flags = THREAD_QUERY_LIMITED_INFORMATION | THREAD_SUSPEND_RESUME;
        for (HANDLE thread = NULL; NT_SUCCESS(NtGetNextThread(thisProcess, thread, flags, 0, 0, &thread));) {
            if (GetThreadId(thread) != thisThread && SuspendThread(thread) != (DWORD)-1) {
                suspended.push_back(thread);
            }
        };
    }

    void resume() {
        for (auto handle : suspended) {
            ResumeThread(handle);
            CloseHandle(handle);
        }
        suspended.clear();
    }

    static NtGetNextThreadFn getNtGetNextThread() {
		auto ntdll = GetModuleHandleW(L"ntdll.dll");
		if (ntdll == NULL)
            throw std::runtime_error("could not get ntdll.dll handle");
        auto fn = GetProcAddress(ntdll, "NtGetNextThread");
		if (fn == NULL)
            throw std::runtime_error("could not find NtGetNextThread in ntdll.dll");
		return reinterpret_cast<NtGetNextThreadFn>(fn);
    }

    std::vector<HANDLE> suspended;
    std::mutex mutex;
};