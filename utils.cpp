#include "utils.h"

#include <algorithm>        // For std::transform
#include <iostream>         // For std::hex
#include <sstream>          // For std::ostringstream
#include <iomanip>          // For setfill
#include <unordered_map>    // For std::unordered_map

#include <conio.h>          // For _getch()

std::wstring toLower(const std::wstring& str) {
    std::wstring result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

std::string bytesToHexString(const uint8_t* bytes, size_t size) {
    std::ostringstream oss;
    for (size_t i = 0; i < size; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i] << " ";
    }
    return oss.str();
}

void pressAnyKeyToExit() {
    std::cout << "Press any key to exit..." << std::endl;
    int _ = _getch(); // Wait for a key press
}

std::unordered_map<std::string, bool> parseCommandLineArgs(int argc, char* argv[]) {
    std::unordered_map<std::string, bool> args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        args[arg] = true;
    }

    return args;
}