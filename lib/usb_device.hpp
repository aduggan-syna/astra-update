#pragma once
#include <device.hpp>

class USBDevice : public Device {
public:
    USBDevice();
    ~USBDevice();

    bool open() override;
    void close() override;

    bool read(uint8_t *data, size_t size) override;
    bool write(const uint8_t *data, size_t size) override;
};