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
    FlashImage(FlashImageType flashImageType, std::string imagePath, std::string bootFirmware, std::string chipName,
        std::string boardName) : m_flashImageType{flashImageType}, m_imagePath{imagePath}, m_bootFirmware{bootFirmware},
        m_chipName{chipName}, m_boardName{boardName}
    {}
    virtual ~FlashImage()
    {}

    virtual int Load() = 0;

    std::string GetBootFirmware() const { return m_bootFirmware; }
    std::string GetChipName() const { return m_chipName; }
    std::string GetBoardName() const { return m_boardName; }

    static std::unique_ptr<FlashImage> FlashImageFactory(std::string imagePath, std::string manifest="");

protected:
    FlashImageType m_flashImageType;
    std::string m_bootFirmware;
    std::string m_chipName;
    std::string m_boardName;
    std::string m_imagePath;
};