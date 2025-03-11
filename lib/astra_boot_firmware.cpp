#include <iostream>
#include <yaml-cpp/yaml.h>
#include <stdexcept>
#include <iomanip>
#include <filesystem>

#include "astra_boot_firmware.hpp"
#include "image.hpp"
#include "astra_log.hpp"

int AstraBootFirmware::LoadManifest(std::string manifestPath)
{
    ASTRA_LOG;

    try {
        YAML::Node manifest = YAML::LoadFile(manifestPath);

        m_id = manifest["id"].as<std::string>();
        m_chipName = manifest["chip"].as<std::string>();
        m_boardName = manifest["board"].as<std::string>();
        m_ubootConsole = manifest["console"].as<std::string>() == "uart" ? ASTRA_UBOOT_CONSOLE_UART : ASTRA_UBOOT_CONSOLE_USB;
        m_uEnvSupport = manifest["uenv_support"].as<bool>();
        m_vendorId = std::stoi(manifest["vendor_id"].as<std::string>(), nullptr, 16);
        m_productId = std::stoi(manifest["product_id"].as<std::string>(), nullptr, 16);

        std::string secureBootString = manifest["secure_boot"].as<std::string>();
        std::transform(secureBootString.begin(), secureBootString.end(), secureBootString.begin(), ::tolower);
        m_secureBootVersion = secureBootString == "gen2" ? ASTRA_SECURE_BOOT_V2 : ASTRA_SECURE_BOOT_V3;

        std::string memoryLayoutString = manifest["memory_layout"].as<std::string>();
        std::transform(memoryLayoutString.begin(), memoryLayoutString.end(), memoryLayoutString.begin(), ::tolower);
        if (memoryLayoutString == "1gb") {
            m_memoryLayout = ASTRA_MEMORY_LAYOUT_1GB;
        } else if (memoryLayoutString == "2gb") {
            m_memoryLayout = ASTRA_MEMORY_LAYOUT_2GB;
        } else if (memoryLayoutString == "3gb") {
            m_memoryLayout = ASTRA_MEMORY_LAYOUT_3GB;
        } else if (memoryLayoutString == "4gb") {
            m_memoryLayout = ASTRA_MEMORY_LAYOUT_4GB;
        } else {
            throw std::runtime_error("Invalid memory layout");
        }

        log(ASTRA_LOG_LEVEL_INFO) << "Loaded boot firmware: " << m_chipName << " " << m_boardName << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "ID: " << m_id << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "Secure boot version: " << (m_secureBootVersion == ASTRA_SECURE_BOOT_V2 ? "gen2" : "genx") << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "Vendor ID: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << m_vendorId << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "Product ID: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << m_productId << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "U-Boot console: " << (m_ubootConsole == ASTRA_UBOOT_CONSOLE_UART ? "UART" : "USB") << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "uEnv support: " << (m_uEnvSupport ? "true" : "false") << endLog;
    } catch (const YAML::BadFile& e) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Unable to open the manifest file: " << e.what() << endLog;
        return -1;
    } catch (const std::exception& e) {
        log(ASTRA_LOG_LEVEL_ERROR) << e.what() << endLog;
        return -1;
    }

    return 0;
}

int AstraBootFirmware::Load()
{
    ASTRA_LOG;

    int ret;

    if (std::filesystem::exists(m_path) && std::filesystem::is_directory(m_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(m_path)) {
                log(ASTRA_LOG_LEVEL_DEBUG) << "Found file: " << entry.path() << endLog;
                if (entry.path().filename().string() == "manifest.yaml") {
                    ret = LoadManifest(entry.path().string());
                    if (ret < 0) {
                        return ret;
                    }
                } else {
                    m_images.push_back(Image(entry.path().string(), ASTRA_IMAGE_TYPE_BOOT));
                }
            }
        }

    m_directoryName = std::filesystem::path(m_path).filename().string();
    log(ASTRA_LOG_LEVEL_DEBUG) << "Loaded boot firmware: " << m_directoryName << endLog;

    return 0;
}

AstraBootFirmware::~AstraBootFirmware()
{
    ASTRA_LOG;
}

const std::string AstraBootFirmware::GetFinalBootImage()
{
    ASTRA_LOG;
    std::string finalBootImage;

    if (m_secureBootVersion == ASTRA_SECURE_BOOT_V2) {
        finalBootImage = "minildr.img";
    } else if (m_secureBootVersion == ASTRA_SECURE_BOOT_V3) {
        if (m_uEnvSupport) {
            finalBootImage = "uEnv.txt";
        } else {
            finalBootImage = "gen3_uboot.bin.usb";
        }
    }

    return finalBootImage;
}