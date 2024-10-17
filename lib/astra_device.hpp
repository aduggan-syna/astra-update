#pragma once

class AstraDevice
{
public:
    AstraDevice();
    ~AstraDevice();

private:
    uint_16 m_vendorId;
    uint_16 m_productId;
};