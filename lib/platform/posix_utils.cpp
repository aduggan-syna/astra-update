#include <string>
#include <iostream>
#include <filesystem>
#include <unistd.h>

#ifdef PLATFORM_MACOS
#include <libkern/OSByteOrder.h>
#define htole32(x) OSSwapHostToLittleInt32(x)
#endif

#include "utils.hpp"

std::string MakeTempDirectory()
{
    char temp[] = "/tmp/astra-update-XXXXXX";
    if (mkdtemp(temp) == nullptr) {
        return "";
    }

    return std::string(temp);
}

uint32_t HostToLE(uint32_t val)
{
#ifdef PLATFORM_MACOS
    return OSSwapHostToLittleInt32(val);
#else
    return htole32(val);
#endif
}