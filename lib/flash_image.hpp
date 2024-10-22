#pragma once

#include <string>
#include <memory>

enum FlashImageType {
    FLASH_IMAGE_TYPE_SPI,
    FLASH_IMAGE_TYPE_NAND,
    FLASH_IMAGE_TYPE_EMMC,
};

class FlashImage
{
public:
    FlashImage(FlashImageType flashImageType, std::string bootFirmware, std::string chipName,
        std::string boardName) : m_flashImageType{flashImageType}, m_bootFirmware{bootFirmware},
        m_chipName{chipName}, m_boardName{boardName}
    {}
    virtual ~FlashImage()
    {}

    virtual int Load(std::string path) = 0;

    static std::unique_ptr<FlashImage> FlashImageFactory(std::string manitest);

protected:
    FlashImageType m_flashImageType;
    std::string m_bootFirmware;
    std::string m_chipName;
    std::string m_boardName;
};