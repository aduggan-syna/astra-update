#pragma once

#include <cstdint>
#include <cstddef>

class Device {
public:
    Device()
    {}
    virtual ~Device()
    {}

    virtual bool Open() = 0;
    virtual void Close() = 0;

    virtual bool Read(uint8_t *data, size_t size) = 0;
    virtual bool Write(const uint8_t *data, size_t size) = 0;
};