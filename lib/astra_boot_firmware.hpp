#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "image.hpp"

enum AstraSecureBootVersion {
    ASTRA_SECURE_BOOT_V2,
    ASTRA_SECURE_BOOT_V3,
};

enum AstraUbootConsole {
    ASTRA_UBOOT_CONSOLE_UART,
    ASTRA_UBOOT_CONSOLE_USB,
};

class AstraBootFirmware
{
public:
    AstraBootFirmware(std::string path = "") : m_path{path}
    {}
    ~AstraBootFirmware();

    int Load();

    uint16_t GetVendorId() const { return m_vendorId; }
    uint16_t GetProductId() const { return m_productId; }
    std::string GetChipName() const { return m_chipName; }
    std::string GetBoardName() const { return m_boardName; }
    std::string GetID() const { return m_id; }
    bool GetUEnvSupport() const { return m_uEnvSupport; }
    enum AstraSecureBootVersion GetSecureBootVersion() const { return m_secureBootVersion; }
    enum AstraUbootConsole GetUbootConsole() const { return m_ubootConsole; }

    const std::vector<Image>& GetImages() const { return m_images; }

private:
    std::string m_path;
    std::string m_directoryName;
    std::vector<Image> m_images;
    std::string m_id;
    std::string m_chipName;
    std::string m_boardName;
    bool m_uEnvSupport;
    enum AstraSecureBootVersion m_secureBootVersion;
    enum AstraUbootConsole m_ubootConsole;
    uint16_t m_vendorId;
    uint16_t m_productId;

    int LoadManifest(std::string manifestPath);
};