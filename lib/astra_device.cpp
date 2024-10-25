#include <iostream>

#include "astra_device.hpp"
#include "astra_boot_firmware.hpp"
#include "flash_image.hpp"
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

    int Boot(std::shared_ptr<AstraBootFirmware> firmware) {
        int ret;

        m_statusCallback(ASTRA_DEVICE_STATE_BOOT_START, 0, "Booting device");

#if 0
        ret = m_device->Boot(firmware);
        if (ret < 0) {
            m_statusCallback(ASTRA_DEVICE_STATE_BOOT_FAIL, 0, "Failed to boot device");
            return ret;
        }
#endif

        m_statusCallback(ASTRA_DEVICE_STATE_BOOT_DONE, 100, "Device booted");

        return 0;
    }

    int Update(std::shared_ptr<FlashImage> image) {
        int ret;

        m_statusCallback(ASTRA_DEVICE_STATE_UPDATE_START, 0, "Updating device");

#if 0
        ret = m_device->Update(image);
        if (ret < 0) {
            m_statusCallback(ASTRA_DEVICE_STATE_UPDATE_FAIL, 0, "Failed to update device");
            return ret;
        }
#endif
        m_statusCallback(ASTRA_DEVICE_STATE_UPDATE_DONE, 100, "Device updated");

        return 0;
    }

    int Reset() {
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

int AstraDevice::Boot(std::shared_ptr<AstraBootFirmware> firmware) {
    return pImpl->Boot(firmware);
}

int AstraDevice::Update(std::shared_ptr<FlashImage> image) {
    return pImpl->Update(image);
}

int AstraDevice::Reset() {
    return pImpl->Reset();
}