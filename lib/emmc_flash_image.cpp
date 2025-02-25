#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

    ParseEmmcPartList();

    return 0;
}

void EmmcFlashImage::ParseEmmcPartList()
{
    ASTRA_LOG;

    std::string emmcPartListPath;
    for (const auto& image : m_images) {
        if (image.GetName() == "emmc_part_list") {
            emmcPartListPath = image.GetPath();
            break;
        }
    }

    std::ifstream file(emmcPartListPath);
    std::string line;
    std::string lastEntryName;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string name;
        if (std::getline(iss, name, ',')) {
            name.erase(name.find_last_not_of(" \t\n\r\f\v") + 1);
            lastEntryName = name;
        }
    }

    m_finalImage = lastEntryName;
    log(ASTRA_LOG_LEVEL_DEBUG) << "Final image: " << m_finalImage << endLog;
}