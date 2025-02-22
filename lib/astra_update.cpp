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
#include "astra_log.hpp"
#include "utils.hpp"

class AstraUpdate::AstraUpdateImpl {
public:
    AstraUpdateImpl()
    {
        ASTRA_LOG;

        m_tempDir = MakeTempDirectory();
        if (m_tempDir.empty()) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to create temporary directory" << endLog;
        }
    }

    ~AstraUpdateImpl() {
    }

    int StartDeviceSearch(std::string bootFirmwareId,
        std::function<void(std::shared_ptr<AstraDevice>)> deviceAddedCallback)
    {
        ASTRA_LOG;

        m_deviceAddedCallback = deviceAddedCallback;

        BootFirmwareCollection bootFirmwareCollection = BootFirmwareCollection("/Users/aduggan/syna/astra-usbboot-firmware");
        bootFirmwareCollection.Load();

        m_firmware = std::make_shared<AstraBootFirmware>(bootFirmwareCollection.GetFirmware(bootFirmwareId));
        uint16_t vendorId = m_firmware->GetVendorId();
        uint16_t productId = m_firmware->GetProductId();

        if (m_transport.Init(vendorId, productId,
                std::bind(&AstraUpdateImpl::DeviceAddedCallback, this, std::placeholders::_1)) < 0)
        {
            return 1;
        }

        log(ASTRA_LOG_LEVEL_DEBUG) << "USB transport initialized successfully" << endLog;

        return 0;
    }

    void StopDeviceSearch()
    {
        ASTRA_LOG;

        m_transport.Shutdown();
    }

    void SetBootFirmwarePath(std::string path)
    {
        ASTRA_LOG;

        m_bootFirmwarePath = path;
    }

    std::shared_ptr<AstraBootFirmware> GetBootFirmware()
    {
        ASTRA_LOG;

        return m_firmware;
    }

    void InitializeLogging(AstraLogLevel minLogLevel, const std::string &logPath)
    {
        std::string modifiedLogPath = logPath;
        if (logPath == "") {
            modifiedLogPath = m_tempDir + "/astra_update.log";
        }
        AstraLogStore::getInstance().Open(modifiedLogPath, minLogLevel);
    }

private:
    USBTransport m_transport;
    std::function<void(std::shared_ptr<AstraDevice>)> m_deviceAddedCallback;
    std::string m_bootFirmwarePath;
    std::shared_ptr<AstraBootFirmware> m_firmware;
    std::string m_tempDir;

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device)
    {
        ASTRA_LOG;

        log(ASTRA_LOG_LEVEL_DEBUG) << "Device added AstraUpdateImpl::DeviceAddedCallback" << endLog;
        std::shared_ptr<AstraDevice> astraDevice = std::make_shared<AstraDevice>(std::move(device), m_tempDir);

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

void AstraUpdate::InitializeLogging(AstraLogLevel minLogLevel, const std::string &logPath)
{
    pImpl->InitializeLogging(minLogLevel, logPath);
}
