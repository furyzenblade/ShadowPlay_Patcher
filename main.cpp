#include "utils.h"
#include "memory.h"

#include <iostream>   // For std::cout
#include <array>      // For std::array

// TODO: Use logging lib for nicer (error) messages in the console.

int patchGetWindowDisplayAffinity(HANDLE hProcess) {
    // TODO: Refactor logging
    uintptr_t remoteUser32Base = getRemoteModuleBaseAddress(hProcess, L"USER32.dll");
    if (!remoteUser32Base) {
        std::cout << "Error: Could not get USER32.dll base address" << std::endl;
        return 1;
    }
    std::cout << "Info: Found USER32.dll base address: 0x" << std::hex << remoteUser32Base << std::endl;

    uintptr_t remoteTargetAddress = getExportedFunctionAddress(hProcess, remoteUser32Base, L"USER32.dll", "GetWindowDisplayAffinity");
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
    // TODO: Refactor logging again
    uintptr_t moduleBaseAddress = getRemoteModuleBaseAddress(hProcess, L"KERNEL32.DLL");
    if (!moduleBaseAddress) {
        std::cout << "Error: Could not get KERNEL32.DLL base address" << std::endl;
        return 1;
    }
    std::cout << "Info: Found KERNEL32.DLL base address: 0x" << std::hex << moduleBaseAddress << std::endl;

    uintptr_t functionAddress = getExportedFunctionAddress(hProcess, moduleBaseAddress, L"KERNEL32.DLL", "Module32FirstW");
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
