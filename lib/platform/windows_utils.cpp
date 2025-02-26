#include "utils.hpp"
#include <windows.h>
#include <stdexcept>
#include <iostream>
#include <string>

#include <stdint.h>

std::string MakeTempDirectory()
{
    char tempPath[MAX_PATH];
    if (GetTempPath(MAX_PATH, tempPath) == 0)
    {
        throw std::runtime_error("Failed to get temp path");
    }

    char tempDir[MAX_PATH];
    if (GetTempFileName(tempPath, "TMP", 0, tempDir) == 0)
    {
        throw std::runtime_error("Failed to get temp file name");
    }

    if (!CreateDirectory(tempDir, NULL))
    {
        throw std::runtime_error("Failed to create temp directory");
    }

    return std::string(tempDir);
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