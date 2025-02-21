#include <string>
#include <iostream>
#include <filesystem>
#include <unistd.h>

#include "utils.hpp"

std::string MakeTempDirectory()
{
    char temp[] = "/tmp/astra-update-XXXXXX";
    if (mkdtemp(temp) == nullptr) {
        return "";
    }

    return std::string(temp);
}

void RemoveTempDirectory(const std::string &path)
{
    std::filesystem::remove_all(path);
}