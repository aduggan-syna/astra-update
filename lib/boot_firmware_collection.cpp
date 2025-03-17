#include <filesystem>
#include <iostream>
#include "boot_firmware_collection.hpp"
#include "astra_boot_firmware.hpp"
#include "image.hpp"
#include "astra_log.hpp"

void BootFirmwareCollection::LoadFirmwareImage(const std::filesystem::path &path)
{
    ASTRA_LOG;

    if (std::filesystem::exists(path / "manifest.yaml")) {
        AstraBootFirmware firmware{path.string()};

        if(firmware.Load()) {
            m_firmwares.push_back(std::make_shared<AstraBootFirmware>(firmware));
        }
    }
}

void BootFirmwareCollection::Load()
{
    ASTRA_LOG;

    log(ASTRA_LOG_LEVEL_DEBUG) << "Loading boot firmwares from " << m_path << endLog;

    std::filesystem::path dir(m_path);

    if (std::filesystem::exists(dir)) {
        if (std::filesystem::is_directory(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (std::filesystem::is_directory(entry.path())) {
                    LoadFirmwareImage(entry.path());
                }
            }
        } else {
            LoadFirmwareImage(dir);
        }
    } else {
        throw std::invalid_argument("Firmware directory " + m_path + " not found");
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

std::vector<std::shared_ptr<AstraBootFirmware>> BootFirmwareCollection::GetFirmwaresForChip(std::string chipName,
    AstraSecureBootVersion secureBoot, AstraMemoryLayout memoryLayout, std::string boardName) const
{
    ASTRA_LOG;

    std::vector<std::shared_ptr<AstraBootFirmware>> firmwares;

    for (const auto& firmware : m_firmwares) {
        if (firmware->GetChipName() == chipName && firmware->GetSecureBootVersion() == secureBoot
          && firmware->GetMemoryLayout() == memoryLayout)
        {
            if (boardName.empty()) {
                firmwares.push_back(firmware);
            } else if (firmware->GetBoardName() == boardName) {
                firmwares.push_back(firmware);
            }
        }
    }

    return firmwares;
}

BootFirmwareCollection::~BootFirmwareCollection()
{
    ASTRA_LOG;
}