#include <iostream>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <filesystem>
#include <condition_variable>
#include "astra_device.hpp"
#include "astra_update.hpp"
#include "boot_firmware_collection.hpp"
#include "usb_transport.hpp"
#include "image.hpp"
#include "astra_log.hpp"
#include "utils.hpp"

#if PLATFORM_WINDOWS
#include "win_usb_transport.hpp"
#endif

class AstraUpdate::AstraUpdateImpl {
public:
    AstraUpdateImpl(std::shared_ptr<FlashImage> flashImage,
        std::string bootFirmwarePath,
        std::function<void(AstraUpdateResponse)> responseCallback,
        bool updateContinuously,
        AstraLogLevel minLogLevel, const std::string &logPath,
        const std::string &tempDir)
        : m_flashImage(flashImage), m_bootFirmwarePath{bootFirmwarePath},
        m_responseCallback{responseCallback} ,m_updateContinuously{updateContinuously}
    {
        if (tempDir.empty()) {
            m_tempDir = MakeTempDirectory();
            if (m_tempDir.empty()) {
                m_tempDir = "./";
            }
        } else {
            m_tempDir = tempDir;
            //std::filesystem::remove_all(m_tempDir);
            std::filesystem::create_directories(m_tempDir);
        }

        std::string modifiedLogPath = logPath;
        if (logPath == "") {
            modifiedLogPath = m_tempDir + "/astra_update.log";
        }
        AstraLogStore::getInstance().Open(modifiedLogPath, minLogLevel);

        ASTRA_LOG;

        log(ASTRA_LOG_LEVEL_INFO) << "final image: " << m_flashImage->GetFinalImage() << endLog;
    }

    ~AstraUpdateImpl()
    {
        ASTRA_LOG;

        AstraLogStore::getInstance().Close();
    }

    int Init()
    {
        ASTRA_LOG;

        BootFirmwareCollection bootFirmwareCollection = BootFirmwareCollection(m_bootFirmwarePath);
        bootFirmwareCollection.Load();

        if (m_flashImage->GetBootFirmwareId().empty()) {
            // No boot firmware specified in the flash image
            // Try to find the best firmware based on other properties
            if (m_flashImage->GetChipName().empty()) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Chip name and boot firmware ID missing!" << endLog;
                m_responseCallback({UpdateResponse{ASTRA_UPDATE_STATUS_FAILURE, "Chip name and boot firmware ID missing!"}});
                return 1;
            }

            std::vector<std::shared_ptr<AstraBootFirmware>> firmwares = bootFirmwareCollection.GetFirmwaresForChip(m_flashImage->GetChipName(),
                m_flashImage->GetSecureBootVersion(), m_flashImage->GetMemoryLayout(), m_flashImage->GetBoardName());
            if (firmwares.size() == 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "No boot firmware found for chip: " << m_flashImage->GetChipName() << endLog;
                m_responseCallback({UpdateResponse{ASTRA_UPDATE_STATUS_FAILURE, "No boot firmware found for chip: " + m_flashImage->GetChipName()}});
                return 1;
            } else if (firmwares.size() > 1) {
                m_firmware = firmwares[0];
                for (const auto& firmware : firmwares) {
                    log(ASTRA_LOG_LEVEL_INFO) << "Boot firmware: " << firmware->GetChipName() << " " << firmware->GetBoardName() << endLog;
                    if (firmware->GetUEnvSupport()) {
                        // Boot firmware with uEnv support is preferred
                        m_firmware = firmware;
                        break;
                    }
                    if (firmware->GetUbootConsole() == ASTRA_UBOOT_CONSOLE_USB) {
                        // Boot firmware with USB console is preferred over UART
                        // But only if there is no uEnv support
                        m_firmware = firmware;
                    }
                }
            } else {
                // Try the only option
                m_firmware = firmwares[0];
            }
        } else {
            // Exact boot firmware specified
            m_firmware = std::make_shared<AstraBootFirmware>(bootFirmwareCollection.GetFirmware(m_flashImage->GetBootFirmwareId()));
        }

        if (m_firmware == nullptr) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Boot firmware not found" << endLog;
            m_responseCallback({UpdateResponse{ASTRA_UPDATE_STATUS_FAILURE, "Boot firmware not set found (Call ValidateBootFirmware first)"}});
            return 1;
        }

        uint16_t vendorId = m_firmware->GetVendorId();
        uint16_t productId = m_firmware->GetProductId();

#if PLATFORM_WINDOWS
        m_transport = std::make_unique<WinUSBTransport>();
#else
        m_transport = std::make_unique<USBTransport>();
#endif

        if (m_transport->Init(vendorId, productId,
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

        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& device : m_devices) {
            device->Close();
        }
        m_devices.clear();
        m_transport->Shutdown();
    }

    std::shared_ptr<AstraBootFirmware> GetBootFirmware()
    {
        ASTRA_LOG;

        return m_firmware;
    }

private:
    std::unique_ptr<USBTransport> m_transport;
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

        log(ASTRA_LOG_LEVEL_DEBUG) << "Updating device" << endLog;

        if (astraDevice) {
            astraDevice->SetStatusCallback(m_responseCallback);

            log(ASTRA_LOG_LEVEL_DEBUG) << "Calling boot" << endLog;
            int ret = astraDevice->Boot(m_firmware);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to boot device" << endLog;
                m_responseCallback({ DeviceResponse{astraDevice->GetDeviceName(), ASTRA_DEVICE_STATUS_BOOT_FAIL, 0, "", "Failed to Boot Device"}});
                return;
            }

            log(ASTRA_LOG_LEVEL_DEBUG) << "calling from Update" << endLog;
            ret = astraDevice->Update(m_flashImage);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to update device" << endLog;
                return;
            }

            log(ASTRA_LOG_LEVEL_DEBUG) << "calling from WaitForCompletion" << endLog;
            ret = astraDevice->WaitForCompletion();
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to wait for completion" << endLog;
                return;
            }

            log(ASTRA_LOG_LEVEL_DEBUG) << "returned from WaitForCompletion" << endLog;
            AstraDeviceStatus status = astraDevice->GetDeviceStatus();
            log(ASTRA_LOG_LEVEL_DEBUG) << "Device status: " << AstraDevice::AstraDeviceStatusToString(status) << endLog;
            if (status == ASTRA_DEVICE_STATUS_UPDATE_COMPLETE && !m_updateContinuously) {
                log(ASTRA_LOG_LEVEL_DEBUG) << "Shutting down Astra Update" << endLog;
                m_responseCallback({UpdateResponse{ASTRA_UPDATE_STATUS_SHUTDOWN, "Astra Update shutting down"}});
            }

            astraDevice->Close();
        }
    }

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device)
    {
        ASTRA_LOG;

        // This code is meant to prevent multiple devices from being added
        // it not in continuous mode. But, if the device hangs in the bootloader
        // we want to be able to restart. Disable for now and revisit if needed.
#if 0
        if (m_deviceFound && !m_updateContinuously) {
            log(ASTRA_LOG_LEVEL_DEBUG) << "Device already found" << endLog;
            return;
        }
#endif

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
    AstraLogLevel minLogLevel, const std::string &logPath,
    const std::string &tempDir)
    : pImpl{std::make_unique<AstraUpdateImpl>(flashImage, bootFirmwarePath, responseCallback,
        updateContinuously, minLogLevel, logPath, tempDir)}
{}

AstraUpdate::~AstraUpdate() = default;

int AstraUpdate::Init()
{
    return pImpl->Init();
}

void AstraUpdate::Shutdown()
{
    return pImpl->Shutdown();
}

std::shared_ptr<AstraBootFirmware> AstraUpdate::GetBootFirmware()
{
    return pImpl->GetBootFirmware();
}
