#pragma once

class Device {
public:
    Device();
    ~Device();

    bool open();
    void close();

    bool read(uint8_t *data, size_t size);
    bool write(const uint8_t *data, size_t size);
};