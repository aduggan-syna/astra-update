#pragma once

#include "flash_image.hpp"

class EmmcFlashImage : public FlashImage
{
public:
    EmmcFlashImage(std::string imagePath, std::string bootFirmware, std::string chipName,
            std::string boardName) : FlashImage(FLASH_IMAGE_TYPE_EMMC, imagePath, bootFirmware, chipName, boardName)
    {}
    virtual ~EmmcFlashImage()
    {}

    virtual int Load() override;
};