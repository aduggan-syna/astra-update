#pragma once
#include <cstdint>
#include <cstddef>
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
    bool m_open;
};