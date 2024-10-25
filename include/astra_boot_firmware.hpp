#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum AstraSecureBootVersion {
    ASTRA_SECURE_BOOT_V2,
    ASTRA_SECURE_BOOT_V3,
};

class Image;

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
    enum AstraSecureBootVersion GetSecureBootVersion() const { return m_secureBootVersion; }

private:
    std::string m_path;
    std::string m_directoryName;
    std::vector<std::shared_ptr<Image>> m_images;
    std::string m_id;
    std::string m_chipName;
    std::string m_boardName;
    enum AstraSecureBootVersion m_secureBootVersion;
    uint16_t m_vendorId;
    uint16_t m_productId;

    int LoadManifest(std::string manifestPath);
};