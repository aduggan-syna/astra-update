#include <iostream>
#include <yaml-cpp/yaml.h>
#include <stdexcept>
#include <iomanip>
#include <filesystem>

#include "astra_boot_firmware.hpp"
#include "image.hpp"

int AstraBootFirmware::LoadManifest(std::string manifestPath)
{
    try {
        YAML::Node manifest = YAML::LoadFile(manifestPath);

        m_id = manifest["id"].as<std::string>();
        m_chipName = manifest["chip"].as<std::string>();
        m_boardName = manifest["board"].as<std::string>();
        m_secureBootVersion = manifest["secure_boot"].as<std::string>() == "gen2" ? ASTRA_SECURE_BOOT_V2 : ASTRA_SECURE_BOOT_V3;
        m_ubootConsole = manifest["console"].as<std::string>() == "uart" ? ASTRA_UBOOT_CONSOLE_UART : ASTRA_UBOOT_CONSOLE_USB;
        m_vendorId = std::stoi(manifest["vendor_id"].as<std::string>(), nullptr, 16);
        m_productId = std::stoi(manifest["product_id"].as<std::string>(), nullptr, 16);

        std::cout << "Loaded boot firmware: " << m_chipName << " " << m_boardName << std::endl;
        std::cout << "ID: " << m_id << std::endl;
        std::cout << "Secure boot version: " << (m_secureBootVersion == ASTRA_SECURE_BOOT_V2 ? "gen2" : "gen3") << std::endl;
        std::cout << "Vendor ID: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << m_vendorId << std::endl;
        std::cout << "Product ID: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << m_productId << std::endl;
    } catch (const YAML::BadFile& e) {
        std::cerr << "Error: Unable to open the manifest file: " << e.what() << std::endl;
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

int AstraBootFirmware::Load()
{
    int ret;

    if (std::filesystem::exists(m_path) && std::filesystem::is_directory(m_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(m_path)) {
                std::cout << "Found file: " << entry.path() << std::endl;
                if (entry.path().filename().string() == "manifest.yaml") {
                    ret = LoadManifest(entry.path().string());
                    if (ret < 0) {
                        return ret;
                    }
                } else {
                    m_images.push_back(Image(entry.path().string()));
                }
            }
        }

    m_directoryName = std::filesystem::path(m_path).filename().string();
    std::cout << "Loaded boot firmware: " << m_directoryName << std::endl;

    return 0;
}

AstraBootFirmware::~AstraBootFirmware()
{
}