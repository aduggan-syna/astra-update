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
#include "image.hpp"

class AstraUpdate::AstraUpdateImpl {
public:
    AstraUpdateImpl()
    {
    }

    ~AstraUpdateImpl() {
    }

    int StartDeviceSearch(std::string bootFirmwareId,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
    {
        m_deviceAddedCallback = deviceAddedCallback;

        BootFirmwareCollection bootFirmwareCollection = BootFirmwareCollection("/home/aduggan/astra-usbboot-firmware");
        bootFirmwareCollection.Load();

        m_firmware = std::make_shared<AstraBootFirmware>(bootFirmwareCollection.GetFirmware(bootFirmwareId));
        uint16_t vendorId = m_firmware->GetVendorId();
        uint16_t productId = m_firmware->GetProductId();

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

    void SetBootFirmwarePath(std::string path)
    {
        m_bootFirmwarePath = path;
    }

    std::shared_ptr<AstraBootFirmware> GetBootFirmware()
    {
        return m_firmware;
    }

private:
    USBTransport m_transport;
    std::function<void(std::shared_ptr<AstraDevice>)> m_deviceAddedCallback;
    std::string m_bootFirmwarePath;
    std::shared_ptr<AstraBootFirmware> m_firmware;

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device) {
        std::cout << "Device added AstraUpdateImpl::DeviceAddedCallback" << std::endl;
        std::shared_ptr<AstraDevice> astraDevice = std::make_shared<AstraDevice>(std::move(device));

        m_deviceAddedCallback(astraDevice);
    }
};

AstraUpdate::AstraUpdate() : pImpl{std::make_unique<AstraUpdateImpl>()} {}

AstraUpdate::~AstraUpdate() = default;

int AstraUpdate::StartDeviceSearch(std::string bootFirmwareId,
     std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
{
    return pImpl->StartDeviceSearch(bootFirmwareId, deviceAddedCallback);
}

void AstraUpdate::StopDeviceSearch()
{
    return pImpl->StopDeviceSearch();
}

void AstraUpdate::SetBootFirmwarePath(std::string path)
{
    pImpl->SetBootFirmwarePath(path);
}

std::shared_ptr<AstraBootFirmware> AstraUpdate::GetBootFirmware()
{
    return pImpl->GetBootFirmware();
}