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

    int Update(std::shared_ptr<FlashImage> flashImage,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback);

private:
    class AstraUpdateImpl;
    std::unique_ptr<AstraUpdateImpl> pImpl;
};