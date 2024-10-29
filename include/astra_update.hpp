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

    int StartDeviceSearch(std::shared_ptr<FlashImage> flashImage,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback);
    void StopDeviceSearch();

    int UpdateDevice(std::shared_ptr<AstraDevice> device);

    void SetBootFirmwarePath(std::string path);

private:
    class AstraUpdateImpl;
    std::unique_ptr<AstraUpdateImpl> pImpl;
};