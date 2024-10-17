#include <filesystem>
#include <iostream>
#include "boot_image_collection.hpp"

void BootImageCollection::Load()
{
    std::cout << "Loading boot images from " << m_path << std::endl;

    std::filesystem::path dir(m_path);

    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (std::filesystem::is_directory(entry.path())) {
                std::cout << "Found boot image directory: " << entry.path() << std::endl;
                if (std::filesystem::exists(entry.path() / "manifest.yaml")) {
                    AstraBootImage image{entry.path()};
                    
                    int ret = image.Load();
                    if (ret < 0) {
                        continue;
                    }

                    m_images.push_back(image);
                }
            }
        }
    }
}

std::vector<AstraBootImage> BootImageCollection::GetSupportedImages(uint16_t vendorId, uint16_t productId) const
{
    std::vector<AstraBootImage> images;

    for (const auto& image : m_images) {
        if (image.GetVendorId() == vendorId && image.GetProductId() == productId) {
            images.push_back(image);
        }
    }

    return images;
}

BootImageCollection::~BootImageCollection()
{
}