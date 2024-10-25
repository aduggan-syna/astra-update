#include <iostream>

#include "astra_device.hpp"
#include "console.hpp"
#include "usb_device.hpp"

class AstraDevice::AstraDeviceImpl {
public:
    AstraDeviceImpl(std::unique_ptr<USBDevice> device) : m_device{std::move(device)}
    {}

    int Open(std::function<void(AstraDeviceState, int progress, std::string message)> statusCallback) {
        int ret;
        
        m_statusCallback = statusCallback;

        ret = m_device->Open();
        if (ret < 0) {
            std::cerr << "Failed to open device" << std::endl;
            return ret;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_OPENED, 0, "Device opened");

        return 0;
    }

private:
    std::unique_ptr<USBDevice> m_device;
    AstraDeviceState m_state;
    std::function<void(AstraDeviceState, int progress, std::string message)> m_statusCallback;
};

AstraDevice::AstraDevice(std::unique_ptr<USBDevice> device) : 
    pImpl{std::make_unique<AstraDeviceImpl>(std::move(device))} {}

AstraDevice::~AstraDevice() = default;

int AstraDevice::Open(std::function<void(AstraDeviceState, int progress, std::string message)> statusCallback) {
    return pImpl->Open(statusCallback);
}