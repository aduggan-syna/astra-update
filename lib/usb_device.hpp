#pragma once
#include <cstdint>
#include <cstddef>
#include <thread>
#include <atomic>
#include <libusb-1.0/libusb.h>

#include "device.hpp"

class USBDevice : public Device {
public:
    USBDevice(libusb_device *device);
    ~USBDevice();

    bool Open() override;
    void Close() override;

    bool Read(uint8_t *data, size_t size) override;
    bool Write(const uint8_t *data, size_t size) override;

private:
    libusb_device *m_device;
    libusb_device_handle *m_handle;
    libusb_config_descriptor *m_config;
    struct libusb_transfer *m_inputInterruptXfer;
    struct libusb_transfer *m_outputInterruptXfer;
    std::thread m_deviceThread;
    std::atomic<bool> m_running;
    bool m_open;

    uint8_t m_interruptInEndpoint;
    uint8_t m_interruptOutEndpoint;
    size_t m_interruptInSize;
    size_t m_interruptOutSize;
    uint8_t *m_interruptInBuffer;
    uint8_t *m_interruptOutBuffer;
    libusb_transfer_cb_fn m_inputInterruptCallback;

    void DeviceThread();
};