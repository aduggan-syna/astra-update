#include <iostream>
#include <memory>
#include <functional>
#include <condition_variable>
#include <mutex>

#include "astra_device.hpp"
#include "astra_boot_firmware.hpp"
#include "flash_image.hpp"
#include "console.hpp"
#include "usb_device.hpp"
#include "image.hpp"

class AstraDevice::AstraDeviceImpl {
public:
    AstraDeviceImpl(std::unique_ptr<USBDevice> device) : m_usbDevice{std::move(device)}
    {}

    void SetStatusCallback(std::function<void(AstraDeviceState, int progress, std::string message)> statusCallback) {
        m_statusCallback = statusCallback;
    }

    int Open() {
        int ret;

        ret = m_usbDevice->Open(std::bind(&AstraDeviceImpl::HandleInterrupt, this, std::placeholders::_1, std::placeholders::_2));
        if (ret < 0) {
            std::cerr << "Failed to open device" << std::endl;
            return ret;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_OPENED, 0, "Device opened");

        return 0;
    }

    int Boot(AstraBootFirmware firmware) {
        int ret;

        m_statusCallback(ASTRA_DEVICE_STATE_BOOT_START, 0, "Booting device");

#if 0
        ret = m_usbDevice->Boot(firmware);
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
        ret = m_usbDevice->Update(image);
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
    std::unique_ptr<USBDevice> m_usbDevice;
    AstraDeviceState m_state;
    std::function<void(AstraDeviceState, int progress, std::string message)> m_statusCallback;

    std::condition_variable imageRequestCV;
    std::mutex imageRequestMutex;

    void HandleInterrupt(uint8_t *buf, size_t size) {
        std::cout << "Interrupt received" << std::endl;

        // Handle Image Request

        // Handle Console Data
    }

    void SendImages() {
        // Wait for image request

        // Find image in vector of images

        // Send block and wait for next request
    }
};

AstraDevice::AstraDevice(std::unique_ptr<USBDevice> device) : 
    pImpl{std::make_unique<AstraDeviceImpl>(std::move(device))} {}

AstraDevice::~AstraDevice() = default;

void AstraDevice::SetStatusCallback(std::function<void(AstraDeviceState, int progress, std::string message)> statusCallback) {
    pImpl->SetStatusCallback(statusCallback);
}

int AstraDevice::Open() {
    return pImpl->Open();
}

int AstraDevice::Boot(AstraBootFirmware &firmware) {
    return pImpl->Boot(firmware);
}

int AstraDevice::Update(std::shared_ptr<FlashImage> image) {
    return pImpl->Update(image);
}

int AstraDevice::Reset() {
    return pImpl->Reset();
}