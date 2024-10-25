#include <iostream>
#include <memory>
#include "astra_device.hpp"
#include "astra_update.hpp"
#include "boot_firmware_collection.hpp"
#include "usb_transport.hpp"

class AstraUpdate::AstraUpdateImpl {
public:
    AstraUpdateImpl() : m_bootFirmwarePath{"/home/aduggan/astra_boot"}
    {
    }

    ~AstraUpdateImpl() {
    }

    int Update(std::shared_ptr<FlashImage> flashImage, std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
    {
        BootFirmwareCollection bootFirmwareCollection = BootFirmwareCollection(m_bootFirmwarePath);
        bootFirmwareCollection.Load();

        try {
            m_bootFirmware = bootFirmwareCollection.GetFirmware(flashImage->GetBootFirmwareId());
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }

        if (m_transport.Init(m_bootFirmware.GetVendorId(), m_bootFirmware.GetProductId(), 
                std::bind(&AstraUpdateImpl::DeviceAddedCallback, this, std::placeholders::_1)) < 0)
        {
            return 1;
        }

        std::cout << "USB transport initialized successfully" << std::endl;

        // block waiting for a device to be added


        return 0;
    }

private:
    std::string m_bootFirmwarePath;
    std::string m_flashImage;
    std::vector<std::shared_ptr<AstraDevice>> m_devices;
    std::function<void(std::shared_ptr<AstraDevice>)> m_deviceAddedCallback;
    AstraBootFirmware m_bootFirmware;
    USBTransport m_transport;

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device) {
        std::shared_ptr<AstraDevice> astraDevice = std::make_shared<AstraDevice>(std::move(device));
        m_devices.push_back(astraDevice);
        m_deviceAddedCallback(astraDevice);
    }
};

AstraUpdate::AstraUpdate() : pImpl{std::make_unique<AstraUpdateImpl>()} {}

AstraUpdate::~AstraUpdate() = default;

int AstraUpdate::Update(std::shared_ptr<FlashImage> flashImage,
     std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
{
    return pImpl->Update(flashImage, deviceAddedCallback);
}