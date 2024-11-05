#pragma once

#include <string>         // For std::string and std::wstring
#include <unordered_map>  // For std::unordered_map
#include <cstdint>        // For uint8_t

std::wstring toLower(const std::wstring& str);
std::string bytesToHexString(const uint8_t* bytes, size_t size);
void pressAnyKeyToExit();
std::unordered_map<std::string, bool> parseCommandLineArgs(int argc, char* argv[]);