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

    int StartDeviceSearch(std::shared_ptr<FlashImage> flashImage,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
    {
        m_flashImage = flashImage;
        m_deviceAddedCallback = deviceAddedCallback;

        BootFirmwareCollection bootFirmwareCollection = BootFirmwareCollection("/home/aduggan/astra_boot");
        bootFirmwareCollection.Load();

        m_firmware = bootFirmwareCollection.GetFirmware(flashImage->GetBootFirmwareId());
        uint16_t vendorId = m_firmware.GetVendorId();
        uint16_t productId = m_firmware.GetProductId();

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

    int UpdateDevice(std::shared_ptr<AstraDevice> device)
    {
        std::thread updateThread(&AstraUpdateImpl::UpdateDeviceThread, this, device);
        m_updateThreads.push_back(std::move(updateThread));

        return 0;
    }

    void SetBootFirmwarePath(std::string path)
    {
        m_bootFirmwarePath = path;
    }

private:
    USBTransport m_transport;
    std::function<void(std::shared_ptr<AstraDevice>)> m_deviceAddedCallback;
    std::shared_ptr<FlashImage> m_flashImage;
    std::string m_bootFirmwarePath;
    AstraBootFirmware m_firmware;
    std::vector<std::thread> m_updateThreads;

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device) {
        std::cout << "Device added AstraUpdateImpl::DeviceAddedCallback" << std::endl;
        std::shared_ptr<AstraDevice> astraDevice = std::make_shared<AstraDevice>(std::move(device));

        m_deviceAddedCallback(astraDevice);
    }

    int UpdateDeviceThread(std::shared_ptr<AstraDevice> device)
    {
        int ret;

        ret = device->Open();
        if (ret < 0) {
            std::cerr << "Failed to open device" << std::endl;
            return ret;
        }

        ret = device->Boot(m_firmware); 
        if (ret < 0) {
            std::cerr << "Failed to boot device" << std::endl;
            return ret;
        }

        ret = device->Update(m_flashImage);
        if (ret < 0) {
            std::cerr << "Failed to open device" << std::endl;
            return ret;
        }

        ret = device->Reset();
        if (ret < 0) {
            std::cerr << "Failed to reset device" << std::endl;
            return ret;
        }

        return 0;
    }
};

AstraUpdate::AstraUpdate() : pImpl{std::make_unique<AstraUpdateImpl>()} {}

AstraUpdate::~AstraUpdate() = default;

int AstraUpdate::StartDeviceSearch(std::shared_ptr<FlashImage> flashImage,
     std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
{
    return pImpl->StartDeviceSearch(flashImage, deviceAddedCallback);
}

void AstraUpdate::StopDeviceSearch()
{
    return pImpl->StopDeviceSearch();
}

int AstraUpdate::UpdateDevice(std::shared_ptr<AstraDevice> device)
{
    return pImpl->UpdateDevice(device);
}

void AstraUpdate::SetBootFirmwarePath(std::string path)
{
    pImpl->SetBootFirmwarePath(path);
}