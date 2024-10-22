#include <filesystem>
#include <iostream>
#include "boot_firmware_collection.hpp"

void BootFirmwareCollection::Load()
{
    std::cout << "Loading boot firmwares from " << m_path << std::endl;

    std::filesystem::path dir(m_path);

    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (std::filesystem::is_directory(entry.path())) {
                std::cout << "Found boot firmware directory: " << entry.path() << std::endl;
                if (std::filesystem::exists(entry.path() / "manifest.yaml")) {
                    AstraBootFirmware firmware{entry.path()};
                    
                    int ret = firmware.Load();
                    if (ret < 0) {
                        continue;
                    }

                    m_firmwares.push_back(firmware);
                }
            }
        }
    }
}

std::vector<AstraBootFirmware> BootFirmwareCollection::GetSupportedFirmwares(uint16_t vendorId, uint16_t productId) const
{
    std::vector<AstraBootFirmware> firmwares;

    for (const auto& firmware : m_firmwares) {
        if (firmware.GetVendorId() == vendorId && firmware.GetProductId() == productId) {
            firmwares.push_back(firmware);
        }
    }

    return firmwares;
}

std::vector<std::tuple<uint16_t, uint16_t>> BootFirmwareCollection::GetDeviceIDs() const
{
    std::vector<std::tuple<uint16_t, uint16_t>> deviceIds;

    for (const auto& firmware : m_firmwares) {
        deviceIds.push_back(std::make_tuple(firmware.GetVendorId(), firmware.GetProductId()));
    }

    return deviceIds;
}

BootFirmwareCollection::~BootFirmwareCollection()
{
}