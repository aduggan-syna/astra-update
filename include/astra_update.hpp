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

    int StartDeviceSearch(uint16_t vendorId, uint16_t productId,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback);
    void StopDeviceSearch();

private:
    class AstraUpdateImpl;
    std::unique_ptr<AstraUpdateImpl> pImpl;
};