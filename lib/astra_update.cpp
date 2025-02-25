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
    AstraUpdateImpl(std::shared_ptr<FlashImage> flashImage,
        std::string bootFirmwarePath,
        std::function<void(AstraUpdateResponse)> responseCallback,
        bool updateContinuously,
        AstraLogLevel minLogLevel, const std::string &logPath)
        : m_flashImage(flashImage), m_bootFirmwarePath{bootFirmwarePath},
        m_responseCallback{responseCallback} ,m_updateContinuously{updateContinuously}
    {
        ASTRA_LOG;

        m_tempDir = MakeTempDirectory();
        if (m_tempDir.empty()) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to create temporary directory" << endLog;
        }

        std::string modifiedLogPath = logPath;
        if (logPath == "") {
            modifiedLogPath = m_tempDir + "/astra_update.log";
        }
        AstraLogStore::getInstance().Open(modifiedLogPath, minLogLevel);
    }

    ~AstraUpdateImpl() {
    }

    int StartDeviceSearch(std::string bootFirmwareId)
    {
        ASTRA_LOG;

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

        m_responseCallback({UpdateResponse{ASTRA_UPDATE_STATUS_START, "Device search started"}});

        return 0;
    }

    void Shutdown()
    {
        ASTRA_LOG;

        m_responseCallback({UpdateResponse{ASTRA_UPDATE_STATUS_SHUTDOWN, "Astra Update shutting down"}});

        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& device : m_devices) {
            device->Shutdown();
        }
        m_devices.clear();
        m_transport.Shutdown();
    }

    std::shared_ptr<AstraBootFirmware> GetBootFirmware()
    {
        ASTRA_LOG;

        return m_firmware;
    }

private:
    USBTransport m_transport;
    std::function<void(AstraUpdateResponse)> m_responseCallback;
    std::string m_bootFirmwarePath;
    std::shared_ptr<AstraBootFirmware> m_firmware;
    std::shared_ptr<FlashImage> m_flashImage;
    std::string m_tempDir;
    bool m_updateContinuously = false;
    bool m_deviceFound = false;

    std::vector<std::shared_ptr<AstraDevice>> m_devices;
    std::mutex m_devicesMutex;

    void UpdateAstraDevice(std::shared_ptr<AstraDevice> astraDevice)
    {
        ASTRA_LOG;

        if (astraDevice) {
            astraDevice->SetStatusCallback(m_responseCallback);

            int ret = astraDevice->Boot(m_firmware);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to boot device" << endLog;
            }

            ret = astraDevice->Update(m_flashImage);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to update device" << endLog;
            }

            ret = astraDevice->WaitForCompletion();
        }
    }

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device)
    {
        ASTRA_LOG;

        if (m_deviceFound && !m_updateContinuously) {
            log(ASTRA_LOG_LEVEL_DEBUG) << "Device already found" << endLog;
            return;
        }

        log(ASTRA_LOG_LEVEL_DEBUG) << "Device added AstraUpdateImpl::DeviceAddedCallback" << endLog;
        std::shared_ptr<AstraDevice> astraDevice = std::make_shared<AstraDevice>(std::move(device), m_tempDir);

        std::lock_guard<std::mutex> lock(m_devicesMutex);
        m_deviceFound = true;
        m_devices.push_back(astraDevice);

        std::thread(std::bind(&AstraUpdateImpl::UpdateAstraDevice, this, astraDevice)).detach();
    }
};

AstraUpdate::AstraUpdate(std::shared_ptr<FlashImage> flashImage,
    std::string bootFirmwarePath,
    std::function<void(AstraUpdateResponse)> responseCallback,
    bool updateContinuously,
    AstraLogLevel minLogLevel, const std::string &logPath)
    : pImpl{std::make_unique<AstraUpdateImpl>(flashImage, bootFirmwarePath, responseCallback,
        updateContinuously, minLogLevel, logPath)}
{}

AstraUpdate::~AstraUpdate() = default;

int AstraUpdate::StartDeviceSearch(std::string bootFirmwareId)
{
    return pImpl->StartDeviceSearch(bootFirmwareId);
}

void AstraUpdate::Shutdown()
{
    return pImpl->Shutdown();
}

std::shared_ptr<AstraBootFirmware> AstraUpdate::GetBootFirmware()
{
    return pImpl->GetBootFirmware();
}
