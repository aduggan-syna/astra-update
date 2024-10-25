#include <stdexcept>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include "flash_image.hpp"

#include "emmc_flash_image.hpp"

const std::string FlashImageTypeToString(FlashImageType type) {
    std::string str = "unknown";

    switch (type) {
        case FLASH_IMAGE_TYPE_SPI:
            str = "spi";
        case FLASH_IMAGE_TYPE_NAND:
            str = "nand";
        case FLASH_IMAGE_TYPE_EMMC:
            str = "emmc";
        default:
            break;
    }

    return str;
}

FlashImageType StringToFlashImageType(const std::string& str) {
    if (str == "spi") {
        return FLASH_IMAGE_TYPE_SPI;
    } else if (str == "nand") {
        return FLASH_IMAGE_TYPE_NAND;
    } else if (str == "emmc") {
        return FLASH_IMAGE_TYPE_EMMC;
    } else {
        throw std::invalid_argument("Unknown FlashImageType string");
    }
}

std::shared_ptr<FlashImage> FlashImage::FlashImageFactory(std::string imagePath, std::string manifest)
{
    FlashImageType flashImageType;
    std::string bootFirmware;
    std::string chipName;
    std::string boardName;

    if (manifest == "") {
        manifest = imagePath + "/manifest.yaml";
    }

    try {
        YAML::Node manifestNode = YAML::LoadFile(manifest);

        bootFirmware = manifestNode["boot_firmware"].as<std::string>();
        chipName = manifestNode["chip"].as<std::string>();
        boardName = manifestNode["board"].as<std::string>();
        flashImageType = StringToFlashImageType(manifestNode["image_type"].as<std::string>());
    }
    catch (const YAML::BadFile& e) {
        std::cerr << "Error: Unable to open the manifest file: " << e.what() << std::endl;
        throw std::invalid_argument("Unknown file");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        throw std::invalid_argument("Invalid Manifest");
    }

    switch (flashImageType) {
        case FLASH_IMAGE_TYPE_SPI:
            //return std::make_unique<SpiFlashImage>();
        case FLASH_IMAGE_TYPE_NAND:
            //return std::make_unique<NandFlashImage>();
        case FLASH_IMAGE_TYPE_EMMC:
            return std::make_shared<EmmcFlashImage>(imagePath, bootFirmware, chipName, boardName);
        default:
            throw std::invalid_argument("Unknown FlashImageType");
    }
}