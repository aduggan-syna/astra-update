#include <iostream>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "astra_device.hpp"
#include "astra_update.hpp"
#include "boot_firmware_collection.hpp"
#include "usb_transport.hpp"

class AstraUpdate::AstraUpdateImpl {
public:
    AstraUpdateImpl()
    {
    }

    ~AstraUpdateImpl() {
    }

    int StartDeviceSearch(uint16_t vendorId, uint16_t productId,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
    {
        m_deviceAddedCallback = deviceAddedCallback;

        if (m_transport.Init(vendorId, productId,
                std::bind(&AstraUpdateImpl::DeviceAddedCallback, this, std::placeholders::_1)) < 0)
        {
            return 1;
        }

        std::cout << "USB transport initialized successfully" << std::endl;

        return 0;
    }

    void StopDeviceSearch()
    {
       m_transport.Shutdown();
    }

private:
    USBTransport m_transport;
    std::function<void(std::shared_ptr<AstraDevice>)> m_deviceAddedCallback;

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device) {
        std::cout << "Device added AstraUpdateImpl::DeviceAddedCallback" << std::endl;
        std::shared_ptr<AstraDevice> astraDevice = std::make_shared<AstraDevice>(std::move(device));

        m_deviceAddedCallback(astraDevice);
    }
};

AstraUpdate::AstraUpdate() : pImpl{std::make_unique<AstraUpdateImpl>()} {}

AstraUpdate::~AstraUpdate() = default;

int AstraUpdate::StartDeviceSearch(uint16_t vendorId, uint16_t productId,
     std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
{
    return pImpl->StartDeviceSearch(vendorId, productId, deviceAddedCallback);
}

void AstraUpdate::StopDeviceSearch()
{
    return pImpl->StopDeviceSearch();
}