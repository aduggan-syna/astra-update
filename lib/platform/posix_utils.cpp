#include <string>
#include <iostream>
#include <unistd.h>

#include "utils.hpp"

std::string MakeTempDirerctory()
{
    char temp[] = "/tmp/astra-update-XXXXXX";
    if (mkdtemp(temp) == nullptr) {
        return "";
    }

    return std::string(temp);
}

void RemoveTempDirectory(const std::string &path)
{
    rmdir(path.c_str());
}