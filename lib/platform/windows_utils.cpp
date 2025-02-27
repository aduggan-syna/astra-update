#include "utils.hpp"
#include <windows.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <stdint.h>
#include <sstream>
#include <iomanip>

std::string MakeTempDirectory()
{
    char tempPath[MAX_PATH];
    if (GetTempPath(MAX_PATH, tempPath) == 0)
    {
        throw std::runtime_error("Failed to get temp path");
    }

    // Generate a unique directory name
    std::stringstream ss;
    ss << tempPath << "TMP" << std::setw(8) << std::setfill('0') << GetTickCount();

    std::string tempDir = ss.str();

    if (!CreateDirectory(tempDir.c_str(), NULL))
    {
        throw std::runtime_error("Failed to create temp directory");
    }

    return tempDir;
}

void RemoveTempDirectory(const std::string &path)
{
    if (!RemoveDirectory(path.c_str()))
    {
        throw std::runtime_error("Failed to remove temp directory");
    }
}

uint32_t HostToLE(uint32_t val)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return val;
#else
    return _byteswap_ulong(val);
#endif
}