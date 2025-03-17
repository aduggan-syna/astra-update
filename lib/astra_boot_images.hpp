#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "image.hpp"

enum AstraUbootConsole {
    ASTRA_UBOOT_CONSOLE_UART,
    ASTRA_UBOOT_CONSOLE_USB,
};

class AstraBootImages
{
public:
    AstraBootImages(std::string path = "") : m_path{path}
    {}
    ~AstraBootImages();

    bool Load();

    uint16_t GetVendorId() const { return m_vendorId; }
    uint16_t GetProductId() const { return m_productId; }
    std::string GetChipName() const { return m_chipName; }
    std::string GetBoardName() const { return m_boardName; }
    std::string GetID() const { return m_id; }
    bool GetUEnvSupport() const { return m_uEnvSupport; }
    AstraSecureBootVersion GetSecureBootVersion() const { return m_secureBootVersion; }
    AstraUbootConsole GetUbootConsole() const { return m_ubootConsole; }
    AstraMemoryLayout GetMemoryLayout() const { return m_memoryLayout; }
    const std::vector<Image>& GetImages() const { return m_images; }

    const std::string GetFinalBootImage();

private:
    std::string m_path;
    std::string m_directoryName;
    std::vector<Image> m_images;
    std::string m_id;
    std::string m_chipName;
    std::string m_boardName;
    bool m_uEnvSupport;
    AstraSecureBootVersion m_secureBootVersion;
    AstraUbootConsole m_ubootConsole;
    AstraMemoryLayout m_memoryLayout;
    uint16_t m_vendorId;
    uint16_t m_productId;


    bool LoadManifest(std::string manifestPath);
};