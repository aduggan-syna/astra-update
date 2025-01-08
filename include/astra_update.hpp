#pragma once

#include <string>
#include <memory>
#include <functional>

#include "astra_device.hpp"
#include "flash_image.hpp"

class AstraUpdate {
public:
    AstraUpdate();
    ~AstraUpdate();

    int StartDeviceSearch(std::string bootFirmwareId,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback);
    void StopDeviceSearch();

    void SetBootFirmwarePath(std::string path);

    std::shared_ptr<AstraBootFirmware> GetBootFirmware();

private:
    class AstraUpdateImpl;
    std::unique_ptr<AstraUpdateImpl> pImpl;
};