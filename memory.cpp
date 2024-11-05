#include "memory.h"
#include "utils.h"      // For toLower

#include <iostream>     // For std::cout
// TODO: Consider defining a Result struct instead to return error messages (E.g.: std::optional<Result> and return std::nullopt)
// instead of printing them to the console directly in the function.

#include <tlhelp32.h>   // CreateToolhelp32Snapshot

std::vector<DWORD> getProcessesByName(const std::wstring& processName) {
    std::vector<DWORD> processIDs;
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return processIDs;
    }

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return processIDs;
    }

    std::wstring targetName = toLower(processName);
    do {
        if (toLower(pe32.szExeFile) == targetName) {
            processIDs.push_back(pe32.th32ProcessID);
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return processIDs;
}

bool isModuleLoaded(DWORD processID, const std::wstring& moduleName) {
    HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processID);
    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!Module32First(hModuleSnap, &me32)) {
        CloseHandle(hModuleSnap);
        return false;
    }

    std::wstring targetModuleName = toLower(moduleName);

    do {
        if (toLower(me32.szModule) == targetModuleName || toLower(me32.szExePath) == targetModuleName) {
            CloseHandle(hModuleSnap);
            return true;
        }
    } while (Module32Next(hModuleSnap, &me32));

    CloseHandle(hModuleSnap);
    return false;
}

uintptr_t getRemoteModuleBaseAddress(HANDLE hProcess, const wchar_t* moduleName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(hProcess));
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32 me;
    me.dwSize = sizeof(MODULEENTRY32);
    uintptr_t moduleBase = 0;
    if (Module32First(hSnapshot, &me)) {
        do {
            if (_wcsicmp(me.szModule, moduleName) == 0) {
                moduleBase = (uintptr_t)me.modBaseAddr;
                break;
            }
        } while (Module32Next(hSnapshot, &me));
    }
    CloseHandle(hSnapshot);
    return moduleBase;
}

uintptr_t getExportedFunctionAddress(HANDLE hProcess, uintptr_t moduleBase, const wchar_t* moduleName, const char* functionName) {
    // Load the specified module locally (will just increase reference count if already loaded) 
    // Only used to get a handle to the module
    HMODULE hLocalModule = LoadLibrary(moduleName);
    if (!hLocalModule) return 0;

    // Get the address of the function in the local module
    FARPROC localProcAddress = GetProcAddress(hLocalModule, functionName);
    if (!localProcAddress) {
        FreeLibrary(hLocalModule);
        return 0;
    }

    // Calculate the offset of the function within the local module
    uintptr_t offset = (uintptr_t)localProcAddress - (uintptr_t)hLocalModule;

    // Free the local module (decrease reference count)
    FreeLibrary(hLocalModule);

    // TODO: Maybe refactor to only return the offset
    // Address of the function in the remote module
    return moduleBase + offset;
}

uintptr_t allocateMemoryNearAddress(HANDLE process, uintptr_t desiredAddress, SIZE_T size, DWORD protection, SIZE_T range) {
    /* Default/optional args:
    // DWORD protection     = PAGE_EXECUTE_READWRITE
    // SIZE_T range         = 0x20000000 - 0x2000
    */

    const SIZE_T step = 0x1000; // One page (4KB)
    uintptr_t baseAddress = desiredAddress - range;
    uintptr_t endAddress = desiredAddress + range;

    for (uintptr_t address = baseAddress; address < endAddress; address += step) {
        void* allocatedMemory = VirtualAllocEx(
            process,
            reinterpret_cast<void*>(address),
            size,
            MEM_RESERVE | MEM_COMMIT,
            protection
        );

        if (allocatedMemory != NULL) {
            return reinterpret_cast<uintptr_t>(allocatedMemory);
        }
    }

    return NULL;
}

bool assembleJumpNearInstruction(uint8_t* buffer, uintptr_t sourceAddress, uintptr_t targetAddress) {
    // TODO: Use dynamic byte array or force length of 5 bytes

    // Calculate the relative offset for the jump
    intptr_t jumpOffset = targetAddress - (sourceAddress + 5); // 5 is the size of the JMP instruction

    if (std::abs(jumpOffset) > 0x7FFFFFFF) { // Check if the offset is within 32-bit range
        return false;
    }

    // Assemble the JMP instruction (E9 offset)
    buffer[0] = 0xE9; // JMP opcode
    *reinterpret_cast<int32_t*>(buffer + 1) = static_cast<int32_t>(jumpOffset);

    return true;
}

bool writeMemory(HANDLE hProcess, uintptr_t address, const void* buffer, SIZE_T size) {
    SIZE_T written;
    return WriteProcessMemory(hProcess, reinterpret_cast<void*>(address), buffer, size, &written) && written == size;
}

bool writeMemoryWithProtection(HANDLE hProcess, uintptr_t address, const void* buffer, SIZE_T size) {
    DWORD oldProtect;

    // Change memory protection to allow writing
    if (!VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cout << "Error: Could not change memory protection" << std::endl;
        return false;
    }

    bool success = writeMemory(hProcess, address, buffer, size);

    // Restore the original memory protection
    if (!VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), size, oldProtect, &oldProtect)) {
        std::cout << "Error: Could not restore memory protection" << std::endl;
        return false;
    }

    return success;
}

bool writeMemoryWithProtectionDynamic(HANDLE hProcess, uintptr_t address, const std::vector<uint8_t>& buffer) {
    // TODO: Make overload for writeMemoryWithProtection
    DWORD oldProtect;

    // Change memory protection to allow writing
    if (!VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), buffer.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::wcout << L"Error: Could not change memory protection" << std::endl;
        return false;
    }

    // Write the memory
    bool success = writeMemory(hProcess, address, buffer.data(), buffer.size());

    // Restore the original memory protection
    if (!VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), buffer.size(), oldProtect, &oldProtect)) {
        std::wcout << L"Error: Could not restore memory protection" << std::endl;
        return false;
    }

    return success;
}