#include <iostream>
#include <filesystem>

#include "image.hpp"
#include "emmc_flash_image.hpp"

int EmmcFlashImage::Load()
{
    int ret;

    if (std::filesystem::exists(m_imagePath) && std::filesystem::is_directory(m_imagePath)) {
        m_directoryName = std::filesystem::path(m_imagePath).filename().string();
        for (const auto& entry : std::filesystem::directory_iterator(m_imagePath)) {
                std::cout << "Found file: " << entry.path() << std::endl;
                if ((entry.path().string().find("emmc") != std::string::npos) ||
                    (entry.path().string().find("subimg") != std::string::npos))
                {

                    m_images.push_back(Image(entry.path().string()));
                }
        }
    }

    return 0;
}