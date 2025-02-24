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
    AstraDevice(std::unique_ptr<USBDevice> device, const std::string &tempDir);
    ~AstraDevice();

    void SetStatusCallback(std::function<void(AstraDeviceState, double progress, std::string message)> statusCallback);

    int Boot(std::shared_ptr<AstraBootFirmware> firmware);
    int Update(std::shared_ptr<FlashImage> flashImage);
    int WaitForCompletion();

    int SendToConsole(const std::string &data);
    int ReceiveFromConsole(std::string &data);

private:
    class AstraDeviceImpl;
    std::unique_ptr<AstraDeviceImpl> pImpl;
};

class AstraDeviceStatusResponse
{
public:
    AstraDeviceStatusResponse(AstraDeviceState state, double progress, std::string message)
        : m_state{state}, m_progress{progress}, m_message{message} {}

    AstraDeviceState GetState() const { return m_state; }
    double GetProgress() const { return m_progress; }
    std::string GetMessage() const { return m_message; }
};