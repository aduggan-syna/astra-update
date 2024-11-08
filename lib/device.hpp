#pragma once

#include <cstdint>
#include <cstddef>

class Device {
public:
    Device()
    {}
    virtual ~Device()
    {}

    virtual int Open()
    {
        return 0;
    }
    virtual void Close() = 0;

    virtual int Read(uint8_t *data, size_t size, int *transferred) = 0;
    virtual int Write(const uint8_t *data, size_t size, int *transferred) = 0;
};