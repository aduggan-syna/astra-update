#pragma once

#include <cstdint>

class AstraDevice
{
public:
    AstraDevice();
    ~AstraDevice();

private:
    uint16_t m_vendorId;
    uint16_t m_productId;
};