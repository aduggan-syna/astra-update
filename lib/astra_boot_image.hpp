#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum AstraSecureBootVersion {
    ASTRA_SECURE_BOOT_V2,
    ASTRA_SECURE_BOOT_V3,
};

class AstraBootImage 
{
public:
    AstraBootImage(std::string path) : m_path{path}
    {}
    ~AstraBootImage();

    int Load();

    uint16_t GetVendorId() const { return m_vendorId; }
    uint16_t GetProductId() const { return m_productId; }
    std::string GetChipName() const { return m_chipName; }
    std::string GetBoardName() const { return m_boardName; }
    enum AstraSecureBootVersion GetSecureBootVersion() const { return m_secureBootVersion; }

private:
    std::string m_path;
    std::vector<std::string> m_files;
    std::string m_chipName;
    std::string m_boardName;
    enum AstraSecureBootVersion m_secureBootVersion;
    uint16_t m_vendorId;
    uint16_t m_productId;
};