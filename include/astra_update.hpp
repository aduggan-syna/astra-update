#pragma once

#include <string>
#include <memory>
#include <functional>
#include <variant>

#include "astra_device.hpp"
#include "flash_image.hpp"
#include "astra_log.hpp"

enum AstraUpdateStatus {
    ASTRA_UPDATE_STATUS_START,
    ASTRA_UPDATE_STATUS_INFO,
    ASTRA_UPDATE_STATUS_FAILURE,
    ASTRA_UPDATE_STATUS_SHUTDOWN,
};

class AstraUpdate {
public:
    AstraUpdate(std::shared_ptr<FlashImage> flashImage,
        std::string bootFirmwarePath,
        std::function<void(AstraUpdateResponse)> responseCallback,
        bool updateContinuously = false,
        AstraLogLevel minLogLevel = ASTRA_LOG_LEVEL_WARNING,
        const std::string &logPath = "",
        const std::string &tempDir = ""
    );
    ~AstraUpdate();

    int Init();

    std::shared_ptr<AstraBootFirmware> GetBootFirmware();

    void Shutdown();

private:
    class AstraUpdateImpl;
    std::unique_ptr<AstraUpdateImpl> pImpl;
};

struct UpdateResponse {
    AstraUpdateStatus m_updateStatus;
    std::string m_updateMessage;
};

class AstraUpdateResponse {
public:
    using ResponseVariant = std::variant<UpdateResponse, DeviceResponse>;

    AstraUpdateResponse(UpdateResponse updateResponse)
        : response(updateResponse) {}

    AstraUpdateResponse(DeviceResponse deviceResponse)
        : response(deviceResponse) {}

    bool IsUpdateResponse() const {
        return std::holds_alternative<UpdateResponse>(response);
    }

    bool IsDeviceResponse() const {
        return std::holds_alternative<DeviceResponse>(response);
    }

    const UpdateResponse& GetUpdateResponse() const {
        return std::get<UpdateResponse>(response);
    }

    const DeviceResponse& GetDeviceResponse() const {
        return std::get<DeviceResponse>(response);
    }

private:
    ResponseVariant response;
};