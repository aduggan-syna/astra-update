#pragma once

#include <string>
#include <memory>
#include <functional>
#include <variant>

#include "astra_device.hpp"
#include "flash_image.hpp"
#include "astra_log.hpp"

enum AstraDeviceManagerStatus {
    ASTRA_DEVICE_MANAGER_STATUS_START,
    ASTRA_DEVICE_MANAGER_STATUS_INFO,
    ASTRA_DEVICE_MANAGER_STATUS_FAILURE,
    ASTRA_DEVICE_MANAGER_STATUS_SHUTDOWN,
};

enum AstraDeviceManangerMode {
    ASTRA_DEVICE_MANAGER_MODE_BOOT,
    ASTRA_DEVICE_MANAGER_MODE_UPDATE,
};

class AstraDeviceManager {
public:
    AstraDeviceManager(std::shared_ptr<FlashImage> flashImage,
        std::string bootImagesPath,
        std::function<void(AstraDeviceManagerResponse)> responseCallback,
        bool updateContinuously = false,
        AstraLogLevel minLogLevel = ASTRA_LOG_LEVEL_WARNING,
        const std::string &logPath = "",
        const std::string &tempDir = "",
        bool usbDebug = false
    );
    AstraDeviceManager(std::string bootImagesPath,
        std::string bootCommand,
        std::function<void(AstraDeviceManagerResponse)> responseCallback,
        bool updateContinuously = false,
        AstraLogLevel minLogLevel = ASTRA_LOG_LEVEL_WARNING,
        const std::string &logPath = "",
        const std::string &tempDir = "",
        bool usbDebug = false
    );
    ~AstraDeviceManager();

    void Init();
    bool Shutdown();
    std::string GetLogFile() const;

private:
    class AstraDeviceManagerImpl;
    std::unique_ptr<AstraDeviceManagerImpl> pImpl;
};

struct ManagerResponse {
    AstraDeviceManagerStatus m_managerStatus;
    std::string m_managerMessage;
};

class AstraDeviceManagerResponse {
public:
    using ResponseVariant = std::variant<ManagerResponse, DeviceResponse>;

    AstraDeviceManagerResponse(ManagerResponse managerResponse)
        : response(managerResponse) {}

        AstraDeviceManagerResponse(DeviceResponse deviceResponse)
        : response(deviceResponse) {}

    bool IsDeviceManagerResponse() const {
        return std::holds_alternative<ManagerResponse>(response);
    }

    bool IsDeviceResponse() const {
        return std::holds_alternative<DeviceResponse>(response);
    }

    const ManagerResponse& GetDeviceManagerResponse() const {
        return std::get<ManagerResponse>(response);
    }

    const DeviceResponse& GetDeviceResponse() const {
        return std::get<DeviceResponse>(response);
    }

private:
    ResponseVariant response;
};