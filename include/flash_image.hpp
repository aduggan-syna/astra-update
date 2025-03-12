#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>

#include "image.hpp"

enum FlashImageType {
    FLASH_IMAGE_TYPE_UNKNOWN,
    FLASH_IMAGE_TYPE_SPI,
    FLASH_IMAGE_TYPE_NAND,
    FLASH_IMAGE_TYPE_EMMC,
};

class FlashImage
{
public:
    FlashImage(FlashImageType flashImageType, std::string imagePath, std::string bootFirmwareId, std::string chipName,
        std::string boardName, AstraSecureBootVersion secureBootVersion, AstraMemoryLayout memoryLayout,
        std::map<std::string, std::string> config) : m_flashImageType{flashImageType}, m_imagePath{imagePath}, m_bootFirmwareId{bootFirmwareId},
        m_chipName{chipName}, m_boardName{boardName}, m_secureBootVersion{secureBootVersion}, m_memoryLayout{memoryLayout}, m_config{config}
    {}
    virtual ~FlashImage()
    {}

    virtual int Load() = 0;

    std::string GetBootFirmwareId() const { return m_bootFirmwareId; }
    std::string GetChipName() const { return m_chipName; }
    std::string GetBoardName() const { return m_boardName; }
    std::string GetFlashCommand() const { return m_flashCommand; }
    const std::string &GetFinalImage() const { return m_finalImage; }
    AstraSecureBootVersion GetSecureBootVersion() const { return m_secureBootVersion; }
    AstraMemoryLayout GetMemoryLayout() const { return m_memoryLayout; }
    const std::vector<Image>& GetImages() const { return m_images; }
    FlashImageType GetFlashImageType() const { return m_flashImageType; }

    static std::shared_ptr<FlashImage> FlashImageFactory(std::string imagePath, std::map<std::string, std::string> &config, std::string manifest="");

protected:
    FlashImageType m_flashImageType;
    std::string m_bootFirmwareId;
    std::string m_chipName;
    std::string m_boardName;
    AstraSecureBootVersion m_secureBootVersion;
    AstraMemoryLayout m_memoryLayout;
    std::string m_imagePath;
    std::vector<Image> m_images;
    std::string m_flashCommand;
    std::string m_finalImage;
    std::map<std::string, std::string> m_config;
};

static std::string AstraFlashImageTypeToString(FlashImageType type)
{
    switch (type) {
        case FLASH_IMAGE_TYPE_SPI:
            return "SPI";
        case FLASH_IMAGE_TYPE_NAND:
            return "NAND";
        case FLASH_IMAGE_TYPE_EMMC:
            return "eMMC";
        default:
            return "Unknown";
    }
}