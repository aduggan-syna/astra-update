#pragma once

#include <memory>
#include <cstdint>
#include <functional>

enum AstraDeviceState {
    ASTRA_DEVICE_STATE_OPENED,
    ASTRA_DEVICE_STATE_CLOSED,
    ASTRA_DEVICE_STATE_BOOT_START,
    ASTRA_DEVICE_STATE_BOOT_DONE,
    ASTRA_DEVICE_STATE_BOOT_FAIL,
    ASTRA_DEVICE_STATE_UPDATE_START,
    ASTRA_DEVICE_STATE_UPDATE_DONE,
    ASTRA_DEVICE_STATE_UPDATE_FAIL,
};

class USBDevice;

class AstraDevice
{
public:
    AstraDevice(std::unique_ptr<USBDevice> device);
    ~AstraDevice();

    int Open(std::function<void(AstraDeviceState, int progress, std::string message)> statusCallback);

private:
    class AstraDeviceImpl;
    std::unique_ptr<AstraDeviceImpl> pImpl;
};