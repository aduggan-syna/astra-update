#include <filesystem>
#include <iostream>
#include "boot_images_collection.hpp"
#include "astra_boot_images.hpp"
#include "image.hpp"
#include "astra_log.hpp"

void BootImagesCollection::LoadBootImages(const std::filesystem::path &path)
{
    ASTRA_LOG;

    if (std::filesystem::exists(path / "manifest.yaml")) {
        AstraBootImages bootImages{path.string()};

        if(bootImages.Load()) {
            m_bootImages.push_back(std::make_shared<AstraBootImages>(bootImages));
        }
    }
}

void BootImagesCollection::Load()
{
    ASTRA_LOG;

    log(ASTRA_LOG_LEVEL_DEBUG) << "Loading boot images from " << m_path << endLog;

    std::filesystem::path dir(m_path);

    if (std::filesystem::exists(dir)) {
        if (std::filesystem::is_directory(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (std::filesystem::is_directory(entry.path())) {
                    LoadBootImages(entry.path());
                }
            }
        } else {
            LoadBootImages(dir);
        }
    } else {
        throw std::invalid_argument("Boot Images directory " + m_path + " not found");
    }
}

std::vector<std::tuple<uint16_t, uint16_t>> BootImagesCollection::GetDeviceIDs() const
{
    ASTRA_LOG;

    std::vector<std::tuple<uint16_t, uint16_t>> deviceIds;

    for (const auto& bootImages : m_bootImages) {
        deviceIds.push_back(std::make_tuple(bootImages->GetVendorId(), bootImages->GetProductId()));
    }

    return deviceIds;
}

AstraBootImages &BootImagesCollection::GetBootImages(std::string id) const
{
    ASTRA_LOG;

    for (const auto& bootImages : m_bootImages) {
        if (bootImages->GetID() == id) {
            return *bootImages;
        }
    }

    throw std::runtime_error("Boot Images not found");
}

std::vector<std::shared_ptr<AstraBootImages>> BootImagesCollection::GetBootImagesForChip(std::string chipName,
    AstraSecureBootVersion secureBoot, AstraMemoryLayout memoryLayout, std::string boardName) const
{
    ASTRA_LOG;

    std::vector<std::shared_ptr<AstraBootImages>> bootImages;

    for (const auto& bootImage : m_bootImages) {
        if (bootImage->GetChipName() == chipName && bootImage->GetSecureBootVersion() == secureBoot
          && bootImage->GetMemoryLayout() == memoryLayout)
        {
            if (boardName.empty()) {
                bootImages.push_back(bootImage);
            } else if (bootImage->GetBoardName() == boardName) {
                bootImages.push_back(bootImage);
            }
        }
    }

    return bootImages;
}

BootImagesCollection::~BootImagesCollection()
{
    ASTRA_LOG;
}