#pragma once

#include <memory>
#include <cstdint>
#include <functional>

#include "flash_image.hpp"

enum AstraDeviceState {
    ASTRA_DEVICE_STATE_OPENED,
    ASTRA_DEVICE_STATE_CLOSED,
    ASTRA_DEVICE_STATE_BOOT_START,
    ASTRA_DEVICE_STATE_BOOT_COMPLETE,
    ASTRA_DEVICE_STATE_BOOT_FAIL,
    ASTRA_DEVICE_STATE_UPDATE_START,
    ASTRA_DEVICE_STATE_UPDATE_COMPLETE,
    ASTRA_DEVICE_STATE_UPDATE_FAIL,
    ASTRA_DEVICE_STATE_IMAGE_SEND_START,
    ASTRA_DEVICE_STATE_IMAGE_SEND_PROGRESS,
    ASTRA_DEVICE_STATE_IMAGE_SEND_COMPLETE,
    ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL,
};

class USBDevice;
class AstraBootFirmware;

class AstraDevice
{
public:
    AstraDevice(std::unique_ptr<USBDevice> device);
    ~AstraDevice();

    void SetStatusCallback(std::function<void(AstraDeviceState, double progress, std::string message)> statusCallback);

    int Open(std::shared_ptr<AstraBootFirmware> firmware, std::shared_ptr<FlashImage> image);
    int Boot();
    int Update();
    int Reset();

private:
    class AstraDeviceImpl;
    std::unique_ptr<AstraDeviceImpl> pImpl;
};