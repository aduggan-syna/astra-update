#include <iostream>
#include "astra_update.hpp"
#include "flash_image.hpp"

void DeviceStatusCallback(AstraDeviceState state, int progress, std::string message) {
    std::cout << "Device status: " << state << " Progress: " << progress << " Message: " << message << std::endl;
}

void DeviceAddedCallback(std::shared_ptr<AstraDevice> device) {
    std::cout << "Device added" << std::endl;

    if (device->Open(DeviceStatusCallback) < 0) {
        std::cerr << "Failed to open device" << std::endl;
    }
}

int main() {
    AstraUpdate update;
    std::shared_ptr<FlashImage> flashImage;

    flashImage = FlashImage::FlashImageFactory("/home/aduggan/sl1680_v1.3.0");

    int ret = update.Update(flashImage, DeviceAddedCallback);
    if (ret < 0) {
        std::cerr << "Error initializing Astra Update" << std::endl;
        return 1;
    }
    return 0;
}