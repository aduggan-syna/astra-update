#pragma once

#include <vector>
#include "astra_boot_firmware.hpp"

class BootFirmwareCollection
{
public:
    BootFirmwareCollection(std::string path) : m_path{path}
    {}
    ~BootFirmwareCollection();

    void Load();

    std::vector<AstraBootFirmware> GetSupportedFirmwares(uint16_t vendorId, uint16_t productId) const;
    std::vector<std::tuple<uint16_t, uint16_t>> GetDeviceIDs() const;

private:
    std::string m_path;
    std::vector<AstraBootFirmware> m_firmwares;
};