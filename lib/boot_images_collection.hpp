#pragma once

#include <vector>
#include <memory>
#include "astra_boot_images.hpp"

class BootImagesCollection
{
public:
    BootImagesCollection(std::string path) : m_path{path}
    {}
    ~BootImagesCollection();

    void Load();

    std::vector<std::tuple<uint16_t, uint16_t>> GetDeviceIDs() const;
    AstraBootImages &GetBootImages(std::string id) const;

    std::vector<std::shared_ptr<AstraBootImages>> GetBootImagesForChip(std::string chipName,
        AstraSecureBootVersion secureBoot, AstraMemoryLayout memoryLayout, std::string boardName) const;

private:
    std::string m_path;
    std::vector<std::shared_ptr<AstraBootImages>> m_bootImages;

    void LoadBootImages(const std::filesystem::path &path);

};