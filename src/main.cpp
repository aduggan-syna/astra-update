#include <iostream>
#include <memory>
#include <queue>
#include <condition_variable>

#include "astra_update.hpp"
#include "flash_image.hpp"
#include "astra_device.hpp"
#include "boot_firmware_collection.hpp"
#include "astra_boot_firmware.hpp"

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

    BootFirmwareCollection bootFirmwareCollection = BootFirmwareCollection("/home/aduggan/astra_boot");
    bootFirmwareCollection.Load();

    std::shared_ptr<AstraBootFirmware> firmware = bootFirmwareCollection.GetFirmware(flashImage->GetBootFirmwareId());
    uint16_t vendorId = firmware->GetVendorId();
    uint16_t productId = firmware->GetProductId();

    int ret = update.StartDeviceSearch(vendorId, productId, DeviceAddedCallback);
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
            if (device->Open(DeviceStatusCallback) < 0) {
                std::cerr << "Failed to open device" << std::endl;
            }

            if (device->Boot(firmware) < 0) {
                std::cerr << "Failed to boot device" << std::endl;
            }

            if (device->Update(flashImage) < 0) {
                std::cerr << "Failed to update device" << std::endl;
            }

            if (device->Reset() < 0) {
                std::cerr << "Failed to update device" << std::endl;
            }
        }
    }

    update.StopDeviceSearch();

    return 0;
}