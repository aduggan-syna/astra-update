#include <filesystem>
#include <iostream>
#include "boot_firmware_collection.hpp"
#include "astra_boot_firmware.hpp"
#include "image.hpp"
#include "astra_log.hpp"

void BootFirmwareCollection::Load()
{
    ASTRA_LOG;

    log(ASTRA_LOG_LEVEL_DEBUG) << "Loading boot firmwares from " << m_path << endLog;

    std::filesystem::path dir(m_path);

    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (std::filesystem::is_directory(entry.path())) {
                log(ASTRA_LOG_LEVEL_DEBUG)<< "Found boot firmware directory: " << entry.path() << endLog;
                if (std::filesystem::exists(entry.path() / "manifest.yaml")) {
                    AstraBootFirmware firmware{entry.path()};
                    
                    int ret = firmware.Load();
                    if (ret < 0) {
                        continue;
                    }

                    m_firmwares.push_back(std::make_shared<AstraBootFirmware>(firmware));
                }
            }
        }
    }
}

std::vector<std::tuple<uint16_t, uint16_t>> BootFirmwareCollection::GetDeviceIDs() const
{
    ASTRA_LOG;

    std::vector<std::tuple<uint16_t, uint16_t>> deviceIds;

    for (const auto& firmware : m_firmwares) {
        deviceIds.push_back(std::make_tuple(firmware->GetVendorId(), firmware->GetProductId()));
    }

    return deviceIds;
}

AstraBootFirmware &BootFirmwareCollection::GetFirmware(std::string id) const
{
    ASTRA_LOG;

    for (const auto& firmware : m_firmwares) {
        if (firmware->GetID() == id) {
            return *firmware;
        }
    }

    throw std::runtime_error("Firmware not found");
}

BootFirmwareCollection::~BootFirmwareCollection()
{
    ASTRA_LOG;
}