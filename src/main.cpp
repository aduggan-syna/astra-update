#include <iostream>
#include <memory>
#include <queue>
#include <condition_variable>

#include "astra_update.hpp"
#include "flash_image.hpp"
#include "astra_device.hpp"

std::queue<std::shared_ptr<AstraDevice>> devices;
std::condition_variable devicesCV;
std::mutex devicesMutex;

void DeviceStatusCallback(AstraDeviceState state, int progress, std::string message) {
    std::cout << "Device status: " << state << " Progress: " << progress << " Message: " << message << std::endl;
}

void DeviceAddedCallback(std::shared_ptr<AstraDevice> device) {
    std::cout << "Device added" << std::endl;

    std::lock_guard<std::mutex> lock(devicesMutex);
    devices.push(device);
    devicesCV.notify_one();
}

int main() {
    AstraUpdate update;
    std::shared_ptr<FlashImage> flashImage;

    flashImage = FlashImage::FlashImageFactory("/home/aduggan/sl1680_v1.3.0");

    int ret = update.StartDeviceSearch(flashImage, DeviceAddedCallback);
    if (ret < 0) {
        std::cerr << "Error initializing Astra Update" << std::endl;
        return 1;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(devicesMutex);
        devicesCV.wait(lock, []{ return !devices.empty(); });

        auto device = devices.front();
        devices.pop();

        if (device) {
            device->SetStatusCallback(DeviceStatusCallback);

            ret = update.UpdateDevice(device);
            if (ret < 0) {
                std::cerr << "Failed to update device" << std::endl;
            }
        }
    }

    update.StopDeviceSearch();

    return 0;
}