#include <iostream>
#include <memory>
#include <queue>
#include <condition_variable>

#include "astra_update.hpp"
#include "flash_image.hpp"
#include "astra_device.hpp"

std::queue<AstraUpdateResponse> updateResponses;
std::condition_variable updateResponsesCV;
std::mutex updateResponsesMutex;

void AstraUpdateResponseCallback(AstraUpdateResponse response)
{
    std::lock_guard<std::mutex> lock(updateResponsesMutex);
    updateResponses.push(response);
    updateResponsesCV.notify_one();
}

int main()
{
    std::shared_ptr<FlashImage> flashImage = FlashImage::FlashImageFactory("/Users/aduggan/syna/sl1640_gpiod");

    int ret = flashImage->Load();
    if (ret < 0) {
        std::cerr << "Failed to load flash image" << std::endl;
        return 1;
    }

    AstraUpdate update(flashImage, "/Users/aduggan/syna/astra-usbboot-firmware",
        AstraUpdateResponseCallback, false, ASTRA_LOG_LEVEL_DEBUG, "/Users/aduggan/syna/astra_update.log");

    ret = update.StartDeviceSearch(flashImage->GetBootFirmwareId());
    if (ret < 0) {
        std::cerr << "Error initializing Astra Update" << std::endl;
        return 1;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(updateResponsesMutex);
        updateResponsesCV.wait(lock, []{ return !updateResponses.empty(); });

        auto status = updateResponses.front();
        updateResponses.pop();

        if (status.IsUpdateResponse()) {
            auto updateResponse = status.GetUpdateResponse();
            std::cout << "Update status: " << updateResponse.m_updateStatus << " Message: " << updateResponse.m_updateMessage << std::endl;

            if (updateResponse.m_updateStatus == ASTRA_UPDATE_STATUS_SHUTDOWN) {
                break;
            }
        } else if (status.IsDeviceResponse()) {
            auto deviceResponse = status.GetDeviceResponse();
            std::cout << "Device status: " << AstraDevice::AstraDeviceStatusToString(deviceResponse.m_status) << " Progress: " << deviceResponse.m_progress << " Image: "
                << deviceResponse.m_imageName << " Message: " << deviceResponse.m_message << std::endl;
        }
    }

    return 0;
}