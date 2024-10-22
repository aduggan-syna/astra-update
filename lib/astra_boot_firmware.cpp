#include <iostream>
#include <yaml-cpp/yaml.h>
#include <stdexcept>
#include <iomanip>

#include "astra_boot_firmware.hpp"

int AstraBootFirmware::Load()
{
    try {
        YAML::Node manifest = YAML::LoadFile(m_path + "/manifest.yaml");

        m_id = manifest["id"].as<std::string>();
        m_chipName = manifest["chip"].as<std::string>();
        m_boardName = manifest["board"].as<std::string>();
        m_secureBootVersion = manifest["secure_boot"].as<std::string>() == "gen2" ? ASTRA_SECURE_BOOT_V2 : ASTRA_SECURE_BOOT_V3;
        m_vendorId = std::stoi(manifest["vendor_id"].as<std::string>(), nullptr, 16);
        m_productId = std::stoi(manifest["product_id"].as<std::string>(), nullptr, 16);

        std::cout << "Loaded boot firmware: " << m_chipName << " " << m_boardName << std::endl;
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

AstraBootFirmware::~AstraBootFirmware()
{
}