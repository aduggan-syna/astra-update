#pragma once

#include <memory>
#include <cstdint>
#include <functional>

#include "flash_image.hpp"

enum AstraDeviceStatus {
    ASTRA_DEVICE_STATUS_ADDED,
    ASTRA_DEVICE_STATUS_OPENED,
    ASTRA_DEVICE_STATUS_CLOSED,
    ASTRA_DEVICE_STATUS_BOOT_START,
    ASTRA_DEVICE_STATUS_BOOT_PROGRESS,
    ASTRA_DEVICE_STATUS_BOOT_COMPLETE,
    ASTRA_DEVICE_STATUS_BOOT_FAIL,
    ASTRA_DEVICE_STATUS_UPDATE_START,
    ASTRA_DEVICE_STATUS_UPDATE_PROGRESS,
    ASTRA_DEVICE_STATUS_UPDATE_COMPLETE,
    ASTRA_DEVICE_STATUS_UPDATE_FAIL,
    ASTRA_DEVICE_STATUS_IMAGE_SEND_START,
    ASTRA_DEVICE_STATUS_IMAGE_SEND_PROGRESS,
    ASTRA_DEVICE_STATUS_IMAGE_SEND_COMPLETE,
    ASTRA_DEVICE_STATUS_IMAGE_SEND_FAIL,
};

class USBDevice;
class AstraBootFirmware;
class AstraUpdateResponse;

class AstraDevice
{
public:
    AstraDevice(std::unique_ptr<USBDevice> device, const std::string &tempDir);
    ~AstraDevice();

    void SetStatusCallback(std::function<void(AstraUpdateResponse)> statusCallback);

    int Boot(std::shared_ptr<AstraBootFirmware> firmware);
    int Update(std::shared_ptr<FlashImage> flashImage);
    int WaitForCompletion();

    int SendToConsole(const std::string &data);
    int ReceiveFromConsole(std::string &data);

    void Shutdown();

    static const std::string AstraDeviceStatusToString(AstraDeviceStatus status);

private:
    class AstraDeviceImpl;
    std::unique_ptr<AstraDeviceImpl> pImpl;
};

struct DeviceResponse
{
    std::string m_deviceName;
    AstraDeviceStatus m_status;
    double m_progress;
    std::string m_imageName;
    std::string m_message;
};
