#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <thread>
#include <utility>

#include "index.hpp"
#include "suspend.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <psapi.h>

constexpr Index::CPattern check1s = "7A ?? 75 ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F 11 05";
constexpr Index::CPattern check1l = "0F 8A ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F 11 05";
constexpr Index::CPattern check2 =  "73 ?? 0F 2F ?? 76 ?? 48 8D 15";
constexpr Index::CPattern check3 =  "72 ?? 48 8D 4C 24 ?? E8 ?? ?? ?? ?? 90 48 8B 05 ?? ?? ?? ?? FF D0";

std::span<uint8_t> getExeImage() noexcept {
    MODULEINFO mInfo;
    HANDLE pHandle = GetCurrentProcess();
    HMODULE pModule = GetModuleHandle(NULL);
    if (!GetModuleInformation(pHandle, pModule, &mInfo, sizeof(mInfo)))
        return {};
    return std::span(reinterpret_cast<uint8_t*>(mInfo.lpBaseOfDll),
        mInfo.SizeOfImage);
}

template <typename T, std::convertible_to<T> Src>
inline void writeProtected(auto* p, Src&& src) {
    T value = T(std::forward<Src>(src));
    DWORD oldProtectDiscard;
    VirtualProtect(static_cast<LPVOID>(p), sizeof(value),
        PAGE_EXECUTE_READWRITE, &oldProtectDiscard);
    std::memcpy(static_cast<void*>(p), static_cast<void*>(&value),
        sizeof(value));
}

void patchDbgChecks() try {
    SuspendMutex suspendThreads;
    std::scoped_lock lock{ suspendThreads };
    auto exe = getExeImage();
    auto index = Index::Bytes(exe);
    // Timed check 1, short pattern:
    for (auto i : index.findAll(check1s)) {
        writeProtected<uint8_t>(&exe[i + 1], 0x02);
        writeProtected<uint8_t>(&exe[i + 3], 0x00);
    }
    // Timed check 1, long pattern:
    for (auto i : index.findAll(check1l)) {
        writeProtected<int32_t>(&exe[i + 2], 0x0000'0006);
        writeProtected<int32_t>(&exe[i + 8], 0x0000'0000);
    }
    // Timed check 2:
    for (auto i : index.findAll(check2))
        writeProtected<uint8_t>(&exe[i + 1], 0x00);
    // Timed check 3 (can't edit displacement, this ends up
    // modifying about 10 bytes of actual bytecode):
    for (auto i : index.findAll(check3))
        writeProtected<uint8_t>(&exe[i], 0xEB);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
}
catch (const std::exception& e) {
    OutputDebugStringA(e.what());
}

int DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        std::thread thread{ &patchDbgChecks };
        thread.detach();
    }
    return TRUE;
}
