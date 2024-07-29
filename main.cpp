#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip> // for setfill
#include <algorithm>
#include <array>

#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <conio.h> // For _getch()
#include <unordered_map>

// Utility function
std::wstring toLower(const std::wstring& str) {
    std::wstring result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

void pressAnyKeyToExit() {
    std::cout << "Press any key to exit..." << std::endl;
    int _ = _getch(); // Wait for a key press
}

// Function to parse command-line arguments
std::unordered_map<std::string, bool> parseCommandLineArgs(int argc, char* argv[]) {
    std::unordered_map<std::string, bool> args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        args[arg] = true;
    }

    return args;
}

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

uintptr_t getRemoteModuleHandle(HANDLE hProcess, const wchar_t* moduleName) {
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

uintptr_t getExportedFunction(HANDLE hProcess, uintptr_t hModule, const wchar_t* moduleName, const char* procName) {
    // Load the specified module locally
    HMODULE hLocalModule = LoadLibrary(moduleName);
    if (!hLocalModule) return 0;

    // Get the address of the function in the local module
    FARPROC localProcAddress = GetProcAddress(hLocalModule, procName);
    if (!localProcAddress) {
        FreeLibrary(hLocalModule);
        return 0;
    }

    // Calculate the offset of the function within the local module
    uintptr_t offset = (uintptr_t)localProcAddress - (uintptr_t)hLocalModule;

    // Free the local module
    FreeLibrary(hLocalModule);

    // Return the address of the function in the remote module
    return hModule + offset;
}

uintptr_t allocateMemoryNearAddress(HANDLE process, uintptr_t desiredAddress, SIZE_T size, DWORD protection = PAGE_EXECUTE_READWRITE, SIZE_T range = 0x20000000 - 0x2000) {
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

std::string bytesToHexString(const uint8_t* bytes, size_t size) {
    std::ostringstream oss;
    for (size_t i = 0; i < size; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i] << " ";
    }
    return oss.str();
}

int patchGetWindowDisplayAffinity(HANDLE hProcess) {
    uintptr_t remoteUser32Base = getRemoteModuleHandle(hProcess, L"USER32.dll");
    if (!remoteUser32Base) {
        std::cout << "Error: Could not get USER32.dll base address" << std::endl;
        return 1;
    }
    std::cout << "Info: Found USER32.dll base address: 0x" << std::hex << remoteUser32Base << std::endl;

    uintptr_t remoteTargetAddress = getExportedFunction(hProcess, remoteUser32Base, L"USER32.dll", "GetWindowDisplayAffinity");
    if (!remoteTargetAddress) {
        std::cout << "Error: Could not get remote address of GetWindowDisplayAffinity" << std::endl;
        return 1;
    }
    std::cout << "Info: Found address of GetWindowDisplayAffinity: 0x" << std::hex << remoteTargetAddress << std::endl;

    // Allocate memory in the target process (near the target address)
    uintptr_t allocatedMemory = allocateMemoryNearAddress(hProcess, remoteTargetAddress, 0x1000);
    if (!allocatedMemory) {
        std::cout << "Error: Could not allocate memory near target address" << std::endl;
        return 1;
    }
    std::cout << "Info: Allocated 1kb of memory at: 0x" << std::hex << allocatedMemory << std::endl;

    // Place payload at new memory location
    if (!writeMemoryWithProtectionDynamic(hProcess, allocatedMemory,
        {
            0x48, 0x31, 0xC0,   // xor rax, rax
            0xC3                // ret
        })
    ) {
        std::cout << "Error: Could not write payload to allocated memory region" << std::endl;
    }
    else {
        std::cout << "Info: Payload written successfully to allocated memory region." << std::endl;
    }

    // Assemble the JMP instruction
    std::array<uint8_t, 5> jmpInstructionBytes;
    if (!assembleJumpNearInstruction(jmpInstructionBytes.data(), remoteTargetAddress, allocatedMemory)) {
        std::cout << "Error: Allocated memory address is too far to assemble a jump near to" << std::endl;
        return 1;
    }
    std::cout << "Info: Assembled jump instruction: " << bytesToHexString(jmpInstructionBytes.data(), jmpInstructionBytes.size()) << std::endl;


    // Write the JMP instruction (plus 1 nop for the left over byte)
    std::array<uint8_t, 6> buffer;
    std::copy(jmpInstructionBytes.begin(), jmpInstructionBytes.end(), buffer.begin());
    buffer[5] = 0x90; // NOP instruction

    if (!writeMemoryWithProtection(hProcess, remoteTargetAddress, buffer.data(), buffer.size())) {
        std::cout << "Error: Could not write jump instruction and NOP to target address" << std::endl;
        return 1;
    }
    std::cout << "Info: Placed hook at USER32.GetWindowDisplayAffinity" << std::endl;

    return 0;
}

int patchKernel32Module32FirstW(HANDLE hProcess) {
    uintptr_t moduleBaseAddress = getRemoteModuleHandle(hProcess, L"KERNEL32.DLL");
    if (!moduleBaseAddress) {
        std::cout << "Error: Could not get KERNEL32.DLL base address" << std::endl;
        return 1;
    }
    std::cout << "Info: Found KERNEL32.DLL base address: 0x" << std::hex << moduleBaseAddress << std::endl;

    uintptr_t functionAddress = getExportedFunction(hProcess, moduleBaseAddress, L"KERNEL32.DLL", "Module32FirstW");
    if (!functionAddress) {
        std::cout << "Error: Could not get remote address of Module32FirstW" << std::endl;
        return 1;
    }
    std::cout << "Info: Found address of Module32FirstW: 0x" << std::hex << functionAddress << std::endl;

    // Allocate memory in the target process (near the target address)
    uintptr_t allocatedMemory = allocateMemoryNearAddress(hProcess, functionAddress, 0x1000);
    if (!allocatedMemory) {
        std::cout << "Error: Could not allocate memory near target address" << std::endl;
        return 1;
    }
    std::cout << "Info: Allocated 1kb of memory at: 0x" << std::hex << allocatedMemory << std::endl;

    // Place payload at new memory location
    if (!writeMemoryWithProtectionDynamic(hProcess, allocatedMemory,
        {
            0x48, 0x31, 0xC0,   // xor rax, rax
            0xC3                // ret
        })
    ) {
        std::cout << "Error: Could not write payload to allocated memory region" << std::endl;
    }
    else {
        std::cout << "Info: Payload written successfully to allocated memory region." << std::endl;
    }

    // Assemble the JMP instruction
    std::array<uint8_t, 5> jmpInstructionBytes;
    if (!assembleJumpNearInstruction(jmpInstructionBytes.data(), functionAddress, allocatedMemory)) {
        std::cout << "Error: Allocated memory address is too far to assemble a jump near to" << std::endl;
        return 1;
    }
    std::cout << "Info: Assembled jump instruction: " << bytesToHexString(jmpInstructionBytes.data(), jmpInstructionBytes.size()) << std::endl;

    // Write the JMP instruction (plus 2 nop's for the left over bytes)
    std::array<uint8_t, 7> buffer;
    std::copy(jmpInstructionBytes.begin(), jmpInstructionBytes.end(), buffer.begin());
    buffer[5] = 0x90; // NOP instruction
    buffer[6] = 0x90; // NOP instruction

    if (!writeMemoryWithProtection(hProcess, functionAddress, buffer.data(), buffer.size())) {
        std::cout << "Error: Could not write jump instruction and NOP to target address" << std::endl;
        return 1;
    }
    std::cout << "Info: Placed hook at KERNEL32.Module32FirstW" << std::endl;

    return 0;
}

int mainWrap() {
    // Get the correct nvcontainer.exe process to patch
    std::vector<DWORD> processIDs = getProcessesByName(L"nvcontainer.exe");

    std::vector<DWORD> filteredProcessIDs;
    for (DWORD processID : processIDs) {
        if (isModuleLoaded(processID, L"nvd3dumx.dll")) {
            filteredProcessIDs.push_back(processID);
        }
    }
    if (filteredProcessIDs.size() != 1) {
        std::cout << "Error: Expected exactly one process with the module nvd3dumx.dll loaded, found " << filteredProcessIDs.size() << "." << std::endl;
        return 1;
    }

    DWORD nvcontainerProcessID = filteredProcessIDs[0];
    std::cout << "Info: Correct process has been found. PID: " << nvcontainerProcessID << std::endl;

    // Attach
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, nvcontainerProcessID);
    if (!hProcess) {
        std::cout << "Error: Could not open process" << std::endl;
        return 1;
    }

    int errorCode = 0;

    // Apply 2 patches
    std::cout << std::endl;
    errorCode = patchGetWindowDisplayAffinity(hProcess);
    if (errorCode) {
        std::cout << "Error: Something went wrong while applying the first patch. Exiting now." << std::endl;
        CloseHandle(hProcess);
        return errorCode;
    }

    std::cout << std::endl;
    errorCode = patchKernel32Module32FirstW(hProcess);
    if (errorCode) {
        std::cout << "Error: Something went wrong while applying the second patch. Exiting now." << std::endl;
        CloseHandle(hProcess);
        return errorCode;
    }

    CloseHandle(hProcess);
    return 0;
}

int main(int argc, char* argv[]) {
    auto args = parseCommandLineArgs(argc, argv);
    bool waitForKeyPress = args.find("--no-wait-for-keypress") == args.end();
    
    int errorCode = mainWrap();
    
    if (waitForKeyPress)
        pressAnyKeyToExit();
    return errorCode;
}
