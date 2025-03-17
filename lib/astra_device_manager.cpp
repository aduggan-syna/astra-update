#include <iostream>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <filesystem>
#include <condition_variable>
#include "astra_device.hpp"
#include "astra_device_manager.hpp"
#include "boot_firmware_collection.hpp"
#include "usb_transport.hpp"
#include "image.hpp"
#include "astra_log.hpp"
#include "utils.hpp"

#if PLATFORM_WINDOWS
#include "win_usb_transport.hpp"
#endif

class AstraDeviceManager::AstraDeviceManagerImpl {
public:
    AstraDeviceManagerImpl(std::shared_ptr<FlashImage> flashImage,
        std::string bootFirmwarePath,
        std::function<void(AstraDeviceManagerResponse)> responseCallback,
        bool runContinuously,
        AstraLogLevel minLogLevel, const std::string &logPath,
        const std::string &tempDir, bool usbDebug)
        : m_flashImage(flashImage), m_bootFirmwarePath{bootFirmwarePath},
        m_responseCallback{responseCallback}, m_runContinuously{runContinuously}, m_usbDebug{usbDebug}
    {
        InitializeLogging(minLogLevel, logPath, tempDir);

        ASTRA_LOG;

        m_managerMode = ASTRA_DEVICE_MANAGER_MODE_UPDATE;
    
        m_bootFirmwareId = m_flashImage->GetBootFirmwareId();
        if (m_bootFirmwareId.empty()) {
            log(ASTRA_LOG_LEVEL_INFO) << "No boot firmware ID specified in the flash image" << endLog;
        } else {
            log(ASTRA_LOG_LEVEL_INFO) << "Boot firmware ID: " << m_bootFirmwareId << endLog;
        }

        m_bootCommand = m_flashImage->GetFlashCommand();

        log(ASTRA_LOG_LEVEL_INFO) << "final image: " << m_flashImage->GetFinalImage() << endLog;
    }

    AstraDeviceManagerImpl(std::string bootFirmwardId,
        std::string bootCommand, std::string bootFirmwarePath,
        std::function<void(AstraDeviceManagerResponse)> responseCallback,
        bool runContinuously,
        AstraLogLevel minLogLevel, const std::string &logPath,
        const std::string &tempDir, bool usbDebug = false)
        : m_bootFirmwareId(bootFirmwardId), m_bootCommand{bootCommand}, m_bootFirmwarePath{bootFirmwarePath},
        m_responseCallback{responseCallback}, m_runContinuously{runContinuously}, m_usbDebug{usbDebug}
    {
        InitializeLogging(minLogLevel, logPath, tempDir);

        ASTRA_LOG;

        m_managerMode = ASTRA_DEVICE_MANAGER_MODE_BOOT;
    }

    void Init()
    {
        ASTRA_LOG;

        BootFirmwareCollection bootFirmwareCollection = BootFirmwareCollection(m_bootFirmwarePath);
        bootFirmwareCollection.Load();

        if (m_bootFirmwareId.empty()) {
            // No boot firmware specified.
            // Try to find the best firmware based on other properties
            if (m_flashImage->GetChipName().empty()) {
                throw std::runtime_error("Chip name and boot firmware ID missing!");
            }

            std::vector<std::shared_ptr<AstraBootFirmware>> firmwares = bootFirmwareCollection.GetFirmwaresForChip(m_flashImage->GetChipName(),
                m_flashImage->GetSecureBootVersion(), m_flashImage->GetMemoryLayout(), m_flashImage->GetBoardName());
            if (firmwares.size() == 0) {
                throw std::runtime_error("No boot firmware found for chip: " + m_flashImage->GetChipName());
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
            throw std::runtime_error("Boot firmware not found");
        }

        std::string bootFirmwareDescription = "Boot Firmware: " + m_firmware->GetChipName() + " " + m_firmware->GetBoardName() + " (" + m_firmware->GetID() + ")\n";
        bootFirmwareDescription += "    Secure Boot: " + AstraSecureBootVersionToString(m_firmware->GetSecureBootVersion()) + "\n";
        bootFirmwareDescription += "    Memory Layout: " + AstraMemoryLayoutToString(m_firmware->GetMemoryLayout()) + "\n";
        bootFirmwareDescription += "    U-Boot Console: " + std::string(m_firmware->GetUbootConsole() == ASTRA_UBOOT_CONSOLE_UART ? "UART" : "USB") + "\n";
        bootFirmwareDescription += "    uEnt.txt Support: " + std::string(m_firmware->GetUEnvSupport() ? "enabled" : "disabled");
        ResponseCallback({ManagerResponse{ASTRA_DEVICE_MANAGER_STATUS_INFO, bootFirmwareDescription}});

        uint16_t vendorId = m_firmware->GetVendorId();
        uint16_t productId = m_firmware->GetProductId();

#if PLATFORM_WINDOWS
        m_transport = std::make_unique<WinUSBTransport>(m_usbDebug);
#else
        m_transport = std::make_unique<USBTransport>(m_usbDebug);
#endif

        if (m_transport->Init(vendorId, productId,
                std::bind(&AstraDeviceManagerImpl::DeviceAddedCallback, this, std::placeholders::_1)) < 0)
        {
            throw std::runtime_error("Failed to initialize USB transport");
        }

        log(ASTRA_LOG_LEVEL_DEBUG) << "USB transport initialized successfully" << endLog;

        std::ostringstream os;
        os << "Waiting for Astra Device (" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << vendorId << ":" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << productId << ")";
        ResponseCallback({ManagerResponse{ASTRA_DEVICE_MANAGER_STATUS_START, os.str()}});
    }

    bool Shutdown()
    {
        ASTRA_LOG;

        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& device : m_devices) {
            device->Close();
        }
        m_devices.clear();
        m_transport->Shutdown();
        AstraLogStore::getInstance().Close();

        if (m_removeTempOnClose) {
            try {
                std::filesystem::remove_all(m_tempDir);
            } catch (const std::exception& e) {
                log(ASTRA_LOG_LEVEL_WARNING) << "Failed to remove temp directory: " << e.what() << endLog;
            }
        }

        return m_failureReported;
    }


    std::string GetLogFile() const
    {
        return m_modifiedLogPath;
    }

private:
    std::unique_ptr<USBTransport> m_transport;
    std::function<void(AstraDeviceManagerResponse)> m_responseCallback;
    std::string m_bootFirmwarePath;
    std::shared_ptr<AstraBootFirmware> m_firmware;
    std::shared_ptr<FlashImage> m_flashImage;
    std::string m_bootFirmwareId;
    std::string m_bootCommand;
    std::string m_tempDir;
    AstraDeviceManangerMode m_managerMode;
    bool m_removeTempOnClose = false;
    bool m_runContinuously = false;
    bool m_deviceFound = false;
    bool m_usbDebug = false;
    bool m_failureReported = false;
    std::string m_modifiedLogPath;

    std::vector<std::shared_ptr<AstraDevice>> m_devices;
    std::mutex m_devicesMutex;

    void InitializeLogging(AstraLogLevel minLogLevel, const std::string &logPath, const std::string &tempDir)
    {
        if (tempDir.empty()) {
            m_tempDir = MakeTempDirectory();
            if (m_tempDir.empty()) {
                m_tempDir = "./";
            }
            m_removeTempOnClose = true;
        } else {
            m_tempDir = tempDir;
            std::filesystem::create_directories(m_tempDir);
        }

        m_modifiedLogPath = logPath;
        if (logPath == "") {
            m_modifiedLogPath = m_tempDir + "/astra_device_manager.log";
        }
        AstraLogStore::getInstance().Open(m_modifiedLogPath, minLogLevel);
    }

    void ResponseCallback(AstraDeviceManagerResponse response)
    {
        // If a failure is reported then retain the temp directory containing logs
        if (response.IsDeviceManagerResponse()) {
            if (response.GetDeviceManagerResponse().m_managerStatus == ASTRA_DEVICE_MANAGER_STATUS_FAILURE) {
                m_removeTempOnClose = false;
                m_failureReported = true;
            }
        } else if (response.IsDeviceResponse()) {
            if (response.GetDeviceResponse().m_status == ASTRA_DEVICE_STATUS_BOOT_FAIL ||
                response.GetDeviceResponse().m_status == ASTRA_DEVICE_STATUS_UPDATE_FAIL)
            {
                m_removeTempOnClose = false;
                m_failureReported = true;
            }
        }
        m_responseCallback(response);
    }

    void AstraDeviceThread(std::shared_ptr<AstraDevice> astraDevice)
    {
        ASTRA_LOG;

        log(ASTRA_LOG_LEVEL_DEBUG) << "Booting device device" << endLog;

        if (astraDevice) {
            astraDevice->SetStatusCallback(m_responseCallback);

            log(ASTRA_LOG_LEVEL_DEBUG) << "Calling boot" << endLog;
            int ret = astraDevice->Boot(m_firmware);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to boot device" << endLog;
                ResponseCallback({ DeviceResponse{astraDevice->GetDeviceName(), ASTRA_DEVICE_STATUS_BOOT_FAIL, 0, "", "Failed to Boot Device"}});
                return;
            }

            if (m_managerMode == ASTRA_DEVICE_MANAGER_MODE_UPDATE) {
                log(ASTRA_LOG_LEVEL_DEBUG) << "calling from Update" << endLog;
                ret = astraDevice->Update(m_flashImage);
                if (ret < 0) {
                    log(ASTRA_LOG_LEVEL_ERROR) << "Failed to update device" << endLog;
                    return;
                }
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
            if (status == ASTRA_DEVICE_STATUS_UPDATE_COMPLETE && !m_runContinuously) {
                log(ASTRA_LOG_LEVEL_DEBUG) << "Shutting down Astra Device Manager" << endLog;
                ResponseCallback({ManagerResponse{ASTRA_DEVICE_MANAGER_STATUS_SHUTDOWN, "Astra Device Manager shutting down"}});
            }

            astraDevice->Close();
        }
    }

    void DeviceAddedCallback(std::unique_ptr<USBDevice> device)
    {
        ASTRA_LOG;

        log(ASTRA_LOG_LEVEL_DEBUG) << "Device added AstraDeviceManagerImpl::DeviceAddedCallback" << endLog;
        std::shared_ptr<AstraDevice> astraDevice = std::make_shared<AstraDevice>(std::move(device), m_tempDir, m_bootCommand);

        std::lock_guard<std::mutex> lock(m_devicesMutex);
        m_deviceFound = true;
        m_devices.push_back(astraDevice);

        std::thread(std::bind(&AstraDeviceManagerImpl::AstraDeviceThread, this, astraDevice)).detach();
    }

};

AstraDeviceManager::AstraDeviceManager(std::shared_ptr<FlashImage> flashImage,
    std::string bootFirmwarePath,
    std::function<void(AstraDeviceManagerResponse)> responseCallback,
    bool runContinuously,
    AstraLogLevel minLogLevel, const std::string &logPath,
    const std::string &tempDir, bool usbDebug)
    : pImpl{std::make_unique<AstraDeviceManagerImpl>(flashImage, bootFirmwarePath, responseCallback,
        runContinuously, minLogLevel, logPath, tempDir, usbDebug)}
{}

AstraDeviceManager::AstraDeviceManager(std::string bootFirmwardId,
    std::string bootCommand, std::string bootFirmwarePath,
    std::function<void(AstraDeviceManagerResponse)> responseCallback,
    bool runContinuously,
    AstraLogLevel minLogLevel, const std::string &logPath,
    const std::string &tempDir, bool usbDebug)
    : pImpl{std::make_unique<AstraDeviceManagerImpl>(bootFirmwardId, bootCommand, bootFirmwarePath, responseCallback,
        runContinuously, minLogLevel, logPath, tempDir, usbDebug)}
{}

AstraDeviceManager::~AstraDeviceManager() = default;

void AstraDeviceManager::Init()
{
    pImpl->Init();
}

bool AstraDeviceManager::Shutdown()
{
    return pImpl->Shutdown();
}

std::string AstraDeviceManager::GetLogFile() const
{
    return pImpl->GetLogFile();
}