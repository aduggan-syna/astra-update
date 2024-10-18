#pragma once

#include <vector>
#include "astra_boot_image.hpp"

class BootImageCollection
{
public:
    BootImageCollection(std::string path) : m_path{path}
    {}
    ~BootImageCollection();

    void Load();

    std::vector<AstraBootImage> GetSupportedImages(uint16_t vendorId, uint16_t productId) const;
    std::vector<std::tuple<uint16_t, uint16_t>> GetDeviceIDs() const;

private:
    std::string m_path;
    std::vector<AstraBootImage> m_images;
};