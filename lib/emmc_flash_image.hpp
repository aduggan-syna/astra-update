#pragma once

#include "flash_image.hpp"

class EmmcFlashImage : public FlashImage
{
public:
    EmmcFlashImage(std::string bootFirmware, std::string chipName,
            std::string boardName) : FlashImage(FLASH_IMAGE_TYPE_EMMC, bootFirmware, chipName, boardName)
    {}
    virtual ~EmmcFlashImage()
    {}

    virtual int Load(std::string path) override;
};