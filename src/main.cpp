#include <iostream>
#include <memory>
#include <queue>
#include <condition_variable>
#include <cxxopts.hpp>

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

int main(int argc, char* argv[])
{
    cxxopts::Options options("AstraUpdate", "Astra Update Utility");

    options.add_options()
        ("b,boot-firmware", "Astra Boot Firmware path", cxxopts::value<std::string>()->default_value("/Users/aduggan/syna/astra-usbboot-firmware"))
        ("l,log", "Log file path", cxxopts::value<std::string>()->default_value(""))
        ("d,debug", "Enable debug logging", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print usage")
        ("flash", "Flash image path", cxxopts::value<std::string>());

    options.parse_positional({"flash"});

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    if (!result.count("flash")) {
        std::cerr << "Error: Flash image path is required." << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }

    std::string flashImagePath = result["flash"].as<std::string>();
    std::string bootFirmwarePath = result["boot-firmware"].as<std::string>();
    std::string logFilePath = result["log"].as<std::string>();
    bool debug = result["debug"].as<bool>();
    AstraLogLevel logLevel = debug ?  ASTRA_LOG_LEVEL_DEBUG : ASTRA_LOG_LEVEL_INFO;

    std::shared_ptr<FlashImage> flashImage = FlashImage::FlashImageFactory(flashImagePath);

    int ret = flashImage->Load();
    if (ret < 0) {
        std::cerr << "Failed to load flash image" << std::endl;
        return 1;
    }

    AstraUpdate update(flashImage, bootFirmwarePath, AstraUpdateResponseCallback, false, logLevel, logFilePath);

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

    update.Shutdown();

    return 0;
}