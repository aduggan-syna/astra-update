#pragma once

#include "flash_image.hpp"

class EmmcFlashImage : public FlashImage
{
public:
    EmmcFlashImage(std::string imagePath, std::string bootFirmware, std::string chipName,
            std::string boardName, AstraSecureBootVersion secureBootVersion, AstraMemoryLayout memoryLayout,
            std::map<std::string, std::string> config) : FlashImage(FLASH_IMAGE_TYPE_EMMC, imagePath,
            bootFirmware, chipName, boardName, secureBootVersion, memoryLayout, config)
    {}
    virtual ~EmmcFlashImage()
    {}

    int Load() override;

private:
    void ParseEmmcImageList();
    const std::string m_resetCommand = "; sleep 1; reset";
};