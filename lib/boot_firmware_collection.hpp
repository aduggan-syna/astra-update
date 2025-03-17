#pragma once

#include <vector>
#include <memory>
#include "astra_boot_firmware.hpp"

class BootFirmwareCollection
{
public:
    BootFirmwareCollection(std::string path) : m_path{path}
    {}
    ~BootFirmwareCollection();

    void Load();

    std::vector<std::tuple<uint16_t, uint16_t>> GetDeviceIDs() const;
    AstraBootFirmware &GetFirmware(std::string id) const;

    std::vector<std::shared_ptr<AstraBootFirmware>> GetFirmwaresForChip(std::string chipName,
        AstraSecureBootVersion secureBoot, AstraMemoryLayout memoryLayout, std::string boardName) const;

private:
    std::string m_path;
    std::vector<std::shared_ptr<AstraBootFirmware>> m_firmwares;

    void LoadFirmwareImage(const std::filesystem::path &path);

};