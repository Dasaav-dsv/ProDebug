#include <cstddef>
#include <cstdint>
#include <thread>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <psapi.h>

void OnAttachThread() noexcept;
std::span<uint8_t> GetExeAddressSpace() noexcept;
void StubFunctionPointer(std::span<uint8_t> instruction,
    std::span<uint8_t> addressSpace) noexcept;

int DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    std::thread thread{ &OnAttachThread };
    thread.detach();
    return TRUE;
}

void OnAttachThread() noexcept {
    auto addressSpace = GetExeAddressSpace();
    auto searchSpace = addressSpace;
    while (!searchSpace.empty()) {
        constexpr uint8_t searchPattern[] = {
            0x48, 0x8B, 0x05, 0, 0, 0, 0, // mov rax,[function]
            0xFF, 0xD0                    // call rax
        };
        auto found = std::ranges::search(searchSpace, searchPattern,
            [](uint8_t a, uint8_t b) { return !b || a == b; });
        searchSpace = std::span(found.end(), searchSpace.end());
        StubFunctionPointer(found, addressSpace);
    }
}

std::span<uint8_t> GetExeAddressSpace() noexcept {
    MODULEINFO mInfo;
    HANDLE pHandle = GetCurrentProcess();
    HMODULE pModule = GetModuleHandle(NULL);
    if (!GetModuleInformation(pHandle, pModule, &mInfo, sizeof(mInfo)))
        return {};
    return std::span(reinterpret_cast<uint8_t*>(mInfo.lpBaseOfDll),
        mInfo.SizeOfImage);
}

void StubFunctionPointer(std::span<uint8_t> instruction,
    std::span<uint8_t> addressSpace) noexcept {
    if (instruction.empty()) return;
    uint8_t* pInstruction = &instruction.front();
    int displacement = *reinterpret_cast<int*>(pInstruction + 3);
    auto pFunctionPointer = pInstruction + displacement + 7;
    if (&addressSpace.front() > pFunctionPointer
        || &addressSpace.back() < pFunctionPointer + sizeof(void*) - 1)
        return;
    auto& functionPointer = *reinterpret_cast<void(**)()>(pFunctionPointer);
    functionPointer = [](){};
}