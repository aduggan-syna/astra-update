#include <iostream>
#include <memory>
#include <queue>
#include <condition_variable>
#include <functional>
#include <cxxopts.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/dynamic_progress.hpp>
#include <unordered_map>

#include "astra_update.hpp"
#include "flash_image.hpp"
#include "astra_device.hpp"

// Define a struct to hold the two strings
struct DeviceImageKey {
    std::string deviceName;
    std::string imageName;

    bool operator==(const DeviceImageKey& other) const {
        return deviceName == other.deviceName && imageName == other.imageName;
    }
};

// Implement a custom hash function for the struct
struct DeviceImageKeyHash {
    std::size_t operator()(const DeviceImageKey& key) const {
        return std::hash<std::string>()(key.deviceName) ^ std::hash<std::string>()(key.imageName);
    }
};

std::queue<AstraUpdateResponse> updateResponses;
std::condition_variable updateResponsesCV;
std::mutex updateResponsesMutex;

void AstraUpdateResponseCallback(AstraUpdateResponse response)
{
    std::lock_guard<std::mutex> lock(updateResponsesMutex);
    updateResponses.push(response);
    updateResponsesCV.notify_one();
}

void UpdateProgressBars(DeviceResponse &deviceResponse,
    indicators::DynamicProgress<indicators::ProgressBar> &dynamicProgress,
    std::unordered_map<DeviceImageKey, size_t, DeviceImageKeyHash> &progressBars)
{
    DeviceImageKey key{deviceResponse.m_deviceName, deviceResponse.m_imageName};

    // Ensure a progress bar exists for this image
    if (progressBars.find(key) == progressBars.end()) {
        auto progress_bar = std::make_unique<indicators::ProgressBar>(
            indicators::option::BarWidth{50},
            indicators::option::Start{"["},
            indicators::option::Fill{"="},
            indicators::option::Lead{">"},
            indicators::option::Remainder{" "},
            indicators::option::End{"]"},
            indicators::option::PostfixText{deviceResponse.m_imageName},
            indicators::option::PrefixText{deviceResponse.m_deviceName + ": "},
            indicators::option::ForegroundColor{indicators::Color::green},
            indicators::option::ShowElapsedTime{true},
            indicators::option::ShowRemainingTime{true},
            indicators::option::MaxProgress{100}
        );
        size_t bardId = dynamicProgress.push_back(std::move(progress_bar));
        progressBars[key] = bardId;
    }

    auto& progressBar = dynamicProgress[progressBars[key]];
    progressBar.set_progress(deviceResponse.m_progress);

    if (deviceResponse.m_progress == 100) {
        progressBar.mark_as_completed();
    }
}

void UpdateDebugProgress(DeviceResponse &deviceResponse)
{
    std::cout << "Device: " << deviceResponse.m_deviceName
                << " Image: " << deviceResponse.m_imageName
                << " Progress: " << deviceResponse.m_progress << std::endl;
}

int main(int argc, char* argv[])
{
    cxxopts::Options options("AstraUpdate", "Astra Update Utility");

    options.add_options()
        ("B,boot-firmware-collection", "Astra Boot Firmware path", cxxopts::value<std::string>()->default_value("astra-usbboot-firmware"))
        ("l,log", "Log file path", cxxopts::value<std::string>()->default_value(""))
        ("D,debug", "Enable debug logging", cxxopts::value<bool>()->default_value("false"))
        ("C,continuous", "Enabled updating multiple devices", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print usage")
        ("T,temp-dir", "Temporary directory", cxxopts::value<std::string>()->default_value(""))
        ("f,flash", "Flash image path", cxxopts::value<std::string>()->default_value("eMMCimg"))
        ("b,board", "Board name", cxxopts::value<std::string>())
        ("c,chip", "Chip name", cxxopts::value<std::string>())
        ("M,manifest", "Manifest file path", cxxopts::value<std::string>())
        ("i,boot-firmware-id", "Boot firmware ID", cxxopts::value<std::string>())
        ("t,image-type", "Image type", cxxopts::value<std::string>())
        ("s,secure-boot", "Secure boot version", cxxopts::value<std::string>()->default_value("gen3"))
        ("m,memory-layout", "Memory layout", cxxopts::value<std::string>())
        ("u,usb-debug", "Enable USB debug logging", cxxopts::value<bool>()->default_value("false"));

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    std::string flashImagePath = result["flash"].as<std::string>();
    std::string bootFirmwarePath = result["boot-firmware-collection"].as<std::string>();
    std::string logFilePath = result["log"].as<std::string>();
    std::string tempDir = result["temp-dir"].as<std::string>();
    bool debug = result["debug"].as<bool>();
    bool continuous = result["continuous"].as<bool>();
    AstraLogLevel logLevel = debug ?  ASTRA_LOG_LEVEL_DEBUG : ASTRA_LOG_LEVEL_INFO;
    bool usbDebug = result["usb-debug"].as<bool>();

    std::string manifest = "";
    if (result.count("manifest")) {
        manifest = result["manifest"].as<std::string>();
    }

    std::map<std::string, std::string> config;
    if (result.count("board")) {
        config["board"] = result["board"].as<std::string>();
    }
    if (result.count("chip")) {
        config["chip"] = result["chip"].as<std::string>();
    }
    if (result.count("image-type")) {
        config["image_type"] = result["image-type"].as<std::string>();
    }
    if (result.count("boot-firmware-id")) {
        config["boot_firmware"] = result["boot-firmware-id"].as<std::string>();
    }
    if (result.count("secure-boot")) {
        config["secure_boot"] = result["secure-boot"].as<std::string>();
    }
    if (result.count("memory-layout")) {
        config["memory_layout"] = result["memory-layout"].as<std::string>();
    }

    // DynamicProgress to manage multiple progress bars
    indicators::DynamicProgress<indicators::ProgressBar> dynamicProgress;
    std::unordered_map<DeviceImageKey, size_t, DeviceImageKeyHash> progressBars;

    dynamicProgress.set_option(indicators::option::HideBarWhenComplete{false});

    std::shared_ptr<FlashImage> flashImage = FlashImage::FlashImageFactory(flashImagePath, config, manifest);
    if (flashImage.get() == nullptr) {
        std::cerr << "Failed to create flash image" << std::endl;
        return 1;
    }

    int ret = flashImage->Load();
    if (ret < 0) {
        std::cerr << "Failed to load flash image" << std::endl;
        return 1;
    }

    AstraUpdate update(flashImage, bootFirmwarePath, AstraUpdateResponseCallback, continuous, logLevel, logFilePath, tempDir);

    ret = update.Init();
    if (ret < 0) {
        std::cerr << "Error Starting Astra Update" << std::endl;
        return 1;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(updateResponsesMutex);
        updateResponsesCV.wait(lock, []{ return !updateResponses.empty(); });

        auto status = updateResponses.front();
        updateResponses.pop();

        if (status.IsUpdateResponse()) {
            auto updateResponse = status.GetUpdateResponse();
            std::cout << "Update status: " << updateResponse.m_updateStatus
                      << " Message: " << updateResponse.m_updateMessage << std::endl;

            if (updateResponse.m_updateStatus == ASTRA_UPDATE_STATUS_SHUTDOWN) {
                break;
            }
        } else if (status.IsDeviceResponse()) {
            auto deviceResponse = status.GetDeviceResponse();

            if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_ADDED) {
                std::cout << "Detected Device: " << deviceResponse.m_deviceName << std::endl;
            } else if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_BOOT_START) {
                std::cout << "Booting Device: " << deviceResponse.m_deviceName << std::endl;
            } else if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_BOOT_COMPLETE) {
                std::cout << "Booting " << deviceResponse.m_deviceName << " is complete" << std::endl;
            } else if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_UPDATE_START) {
                std::cout << "Updating Device: " << deviceResponse.m_deviceName << std::endl;
            } else if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_UPDATE_COMPLETE) {
                std::cout << "Device: " << deviceResponse.m_deviceName << " Update Complete" << std::endl;
            } else if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_BOOT_FAIL) {
                std::cout << "Device: " << deviceResponse.m_deviceName << " Boot Failed: " << deviceResponse.m_message << std::endl;
            } else if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_UPDATE_FAIL) {
                std::cout << "Device: " << deviceResponse.m_deviceName << " Update Failed: " << deviceResponse.m_message << std::endl;
            } else if (deviceResponse.m_status == ASTRA_DEVICE_STATUS_IMAGE_SEND_START ||
                deviceResponse.m_status == ASTRA_DEVICE_STATUS_IMAGE_SEND_PROGRESS ||
                deviceResponse.m_status == ASTRA_DEVICE_STATUS_IMAGE_SEND_COMPLETE)
            {
                if (usbDebug) {
                    UpdateDebugProgress(deviceResponse);
                } else {
                    UpdateProgressBars(deviceResponse, dynamicProgress, progressBars);
                }
            }
        }
    }

    update.Shutdown();

    return 0;
}
