#pragma once

#include <string>

std::string MakeTempDirectory();
void RemoveTempDirectory(const std::string &path);

uint32_t HostToLE(uint32_t val);