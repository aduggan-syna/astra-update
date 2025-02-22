#include <iostream>
#include <filesystem>

#include "image.hpp"
#include "emmc_flash_image.hpp"
#include "astra_log.hpp"

int EmmcFlashImage::Load()
{
    ASTRA_LOG;

    int ret;

    if (std::filesystem::exists(m_imagePath) && std::filesystem::is_directory(m_imagePath)) {
        m_directoryName = std::filesystem::path(m_imagePath).filename().string();
        for (const auto& entry : std::filesystem::directory_iterator(m_imagePath)) {
            log(ASTRA_LOG_LEVEL_DEBUG) << "Found file: " << entry.path() << endLog;
                if ((entry.path().string().find("emmc") != std::string::npos) ||
                    (entry.path().string().find("subimg") != std::string::npos))
                {
                    m_images.push_back(Image(entry.path().string()));

                    m_flashCommand = "l2emmc " + m_directoryName;
                }
        }
    }

    return 0;
}