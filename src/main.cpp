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

void DeviceStatusCallback(AstraDeviceState state, double progress, std::string message) {
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

    flashImage = FlashImage::FlashImageFactory("/home/aduggan/sl1680");

    int ret = flashImage->Load();
    if (ret < 0) {
        std::cerr << "Failed to load flash image" << std::endl;
        return 1;
    }

    ret = update.StartDeviceSearch(flashImage->GetBootFirmwareId(), DeviceAddedCallback);
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

            ret = device->Boot(update.GetBootFirmware());
            if (ret < 0) {
                std::cerr << "Failed to boot device" << std::endl;
            }

            ret = device->Update(flashImage);
            if (ret < 0) {
                std::cerr << "Failed to update device" << std::endl;
            }

            ret = device->Complete();
        }
    }

    update.StopDeviceSearch();

    return 0;
}