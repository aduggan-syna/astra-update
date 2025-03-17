#include <filesystem>
#include <iostream>
#include "boot_images_collection.hpp"
#include "astra_boot_image.hpp"
#include "image.hpp"
#include "astra_log.hpp"

void BootImagesCollection::LoadBootImage(const std::filesystem::path &path)
{
    ASTRA_LOG;

    if (std::filesystem::exists(path / "manifest.yaml")) {
        AstraBootImage bootImages{path.string()};

        if(bootImages.Load()) {
            m_bootImages.push_back(std::make_shared<AstraBootImage>(bootImages));
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
                    LoadBootImage(entry.path());
                }
            }
        } else {
            LoadBootImage(dir);
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

AstraBootImage &BootImagesCollection::GetBootImage(std::string id) const
{
    ASTRA_LOG;

    for (const auto& bootImage : m_bootImages) {
        if (bootImage->GetID() == id) {
            return *bootImage;
        }
    }

    throw std::runtime_error("Boot Images not found");
}

std::vector<std::shared_ptr<AstraBootImage>> BootImagesCollection::GetBootImagesForChip(std::string chipName,
    AstraSecureBootVersion secureBoot, AstraMemoryLayout memoryLayout, std::string boardName) const
{
    ASTRA_LOG;

    std::vector<std::shared_ptr<AstraBootImage>> bootImages;

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