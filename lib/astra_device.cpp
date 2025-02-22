#include <atomic>
#include <iostream>
#include <memory>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <cstring>

#ifdef PLATFORM_MACOS
#include <libkern/OSByteOrder.h>
#define htole32(x) OSSwapHostToLittleInt32(x)
#endif

#include "astra_device.hpp"
#include "astra_boot_firmware.hpp"
#include "flash_image.hpp"
#include "astra_console.hpp"
#include "usb_device.hpp"
#include "image.hpp"
#include "utils.hpp"
#include "astra_log.hpp"

class AstraDevice::AstraDeviceImpl {
public:
    AstraDeviceImpl(std::unique_ptr<USBDevice> device) : m_usbDevice{std::move(device)}
    {
        ASTRA_LOG;

        m_tempDir = MakeTempDirectory();
        if (m_tempDir.empty()) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to create temporary directory" << endLog;
        }

        m_console = std::make_unique<AstraConsole>(m_tempDir);
    }

    ~AstraDeviceImpl()
    {
        ASTRA_LOG;

        Shutdown();

        if (m_imageRequestThread.joinable()) {
            m_imageRequestThread.join();
        }
        m_console->Shutdown();

        m_usbDevice->Close();
        //RemoveTempDirectory(m_tempDir);
    }

    void SetStatusCallback(std::function<void(AstraDeviceState, double progress, std::string message)> statusCallback)
    {
        ASTRA_LOG;

        m_statusCallback = statusCallback;
    }

    int Boot(std::shared_ptr<AstraBootFirmware> firmware)
    {
        ASTRA_LOG;

        int ret;

        m_ubootConsole = firmware->GetUbootConsole();
        m_uEnvSupport = firmware->GetUEnvSupport();

        ret = m_usbDevice->Open(std::bind(&AstraDeviceImpl::USBEventHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to open device" << endLog;
            return ret;
        }

        std::ofstream imageFile(m_tempDir + "/" + m_usbPathImageFilename);
        if (!imageFile) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to open 06_IMAGE file" << endLog;
            return -1;
        }

        imageFile << m_usbDevice->GetUSBPath();
        imageFile.close();

        m_usbPathImage = std::make_unique<Image>(m_tempDir + "/"  + m_usbPathImageFilename);
        m_sizeRequestImage = std::make_unique<Image>(m_tempDir + "/" + m_sizeRequestImageFilename);
        m_uEnvImage = std::make_unique<Image>(m_tempDir + "/" + m_uEnvFilename);

        m_state = ASTRA_DEVICE_STATE_OPENED;

        std::vector<Image> firmwareImages = firmware->GetImages();
        m_images->insert(m_images->end(), firmwareImages.begin(), firmwareImages.end());

        m_imageRequestThread= std::thread(std::bind(&AstraDeviceImpl::ImageRequestThread, this));

        return 0;
    }

    int Update(std::shared_ptr<FlashImage> flashImage)
    {
        ASTRA_LOG;

        m_images->insert(m_images->end(), flashImage->GetImages().begin(), flashImage->GetImages().end());
        if (m_uEnvSupport) {
            std::string uEnv = "bootcmd=" + flashImage->GetFlashCommand() + "; reset";
            std::ofstream uEnvFile(m_tempDir + "/" + m_uEnvFilename);
            if (!uEnvFile) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to open uEnv.txt file" << endLog;
                return -1;
            }
            uEnvFile << uEnv;
            uEnvFile.close();
            m_images->push_back(*m_uEnvImage);
        } else if (m_ubootConsole == ASTRA_UBOOT_CONSOLE_USB) {
            if (m_console->WaitForPrompt()) {
                SendToConsole(flashImage->GetFlashCommand() + "\n");
            }
        }

        return 0;
    }

    int WaitForCompletion()
    {
        ASTRA_LOG;

        if (m_uEnvSupport) {
            for (;;) {
                std::unique_lock<std::mutex> lock(m_deviceEventMutex);
                m_deviceEventCV.wait(lock);
                if (m_shutdown.load()) {
                    break;
                }
            }
        } else if (m_ubootConsole == ASTRA_UBOOT_CONSOLE_USB) {
            if (m_console->WaitForPrompt()) {
                SendToConsole("reset\n");
            }
        }

        return 0;
    }

    int SendToConsole(const std::string &data)
    {
        ASTRA_LOG;

        int ret = m_usbDevice->WriteInterruptData(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to send data to console" << endLog;
            return ret;
        }
        return 0;
    }

    int ReceiveFromConsole(std::string &data)
    {
        ASTRA_LOG;

        data = m_console->Get();
        return 0;
    }

private:
    std::unique_ptr<USBDevice> m_usbDevice;
    AstraDeviceState m_state;
    std::function<void(AstraDeviceState, int progress, std::string message)> m_statusCallback;
    std::atomic<bool> m_shutdown{false};
    bool m_uEnvSupport = false;

    std::shared_ptr<std::vector<Image>> m_images = std::make_shared<std::vector<Image>>();

    std::condition_variable m_deviceEventCV;
    std::mutex m_deviceEventMutex;

    std::thread m_imageRequestThread;
    std::condition_variable m_imageRequestCV;
    std::mutex m_imageRequestMutex;
    uint8_t m_imageType;
    std::string m_requestedImageName;

    const std::string m_imageRequestString = "i*m*g*r*q*";
    static constexpr int m_imageBufferSize = (1 * 1024 * 1024) + 4;
    uint8_t m_imageBuffer[m_imageBufferSize];

    std::unique_ptr<AstraConsole> m_console;
    enum AstraUbootConsole m_ubootConsole;

    std::string m_tempDir;
    const std::string m_usbPathImageFilename = "06_IMAGE";
    const std::string m_sizeRequestImageFilename = "07_IMAGE";
    const std::string m_uEnvFilename = "uEnv.txt";
    std::unique_ptr<Image> m_usbPathImage;
    std::unique_ptr<Image> m_sizeRequestImage;
    std::unique_ptr<Image> m_uEnvImage;

    void ImageRequestThread()
    {
        ASTRA_LOG;

        int ret = HandleImageRequests();
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to handle image request" << endLog;
        }
    }

    void HandleInterrupt(uint8_t *buf, size_t size)
    {
        ASTRA_LOG;

        log(ASTRA_LOG_LEVEL_DEBUG) << "Interrupt received: size:" << size << endLog;

        for (size_t i = 0; i < size; ++i) {
            log << std::hex << static_cast<int>(buf[i]) << " ";
        }
        log << std::dec << endLog;

        std::string message(reinterpret_cast<char *>(buf), size);

        auto it = message.find(m_imageRequestString);
        if (it != std::string::npos) {
            std::unique_lock<std::mutex> lock(m_imageRequestMutex);
            it += m_imageRequestString.size();
            m_imageType = buf[it];
            log(ASTRA_LOG_LEVEL_DEBUG) << "Image type: " << std::hex << m_imageType << std::dec << endLog;
            std::string imageName = message.substr(it + 1);

            // Strip off Null character
            size_t end = imageName.find_last_not_of('\0');
            if (end != std::string::npos) {
                m_requestedImageName = imageName.substr(0, end + 1);
            } else {
                m_requestedImageName = imageName;
            }
            log(ASTRA_LOG_LEVEL_DEBUG) << "Requested image name: '" << m_requestedImageName << "'" << endLog;

            m_imageRequestCV.notify_one();
        } else {
            m_console->Append(message);
        }
    }

    void USBEventHandler(USBDevice::USBEvent event, uint8_t *buf, size_t size)
    {
        ASTRA_LOG;

        if (event == USBDevice::USB_DEVICE_EVENT_INTERRUPT) {
            HandleInterrupt(buf, size);
        } else if (event == USBDevice::USB_DEVICE_EVENT_NO_DEVICE) {
            // device disappeared
            Shutdown();
        }
    }

    int UpdateImageSizeRequestFile(uint32_t fileSize)
    {
        ASTRA_LOG;

        if (m_imageType > 0x79)
        {
            std::string imageName = m_sizeRequestImage->GetPath();
            FILE *sizeFile = fopen(imageName.c_str(), "w");
            if (!sizeFile) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to open " << imageName << " file" << endLog;
                return -1;
            }
            log(ASTRA_LOG_LEVEL_DEBUG) << "Writing image size to 07_IMAGE: " << fileSize << endLog;

            if (fwrite(&fileSize, sizeof(fileSize), 1, sizeFile) != 1) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to write image size to file" << endLog;
                fclose(sizeFile);
                return -1;
            }
            fclose(sizeFile);
        }

        return 0;
    }

    int SendImage(Image *image)
    {
        ASTRA_LOG;

        int ret;
        int totalTransferred = 0;
        int transferred = 0;

        ret = image->Load();
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to load image" << endLog;
            return ret;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_START, 0, image->GetName());

        uint32_t imageSizeLE = htole32(image->GetSize());
        std::memcpy(m_imageBuffer, &imageSizeLE, sizeof(imageSizeLE));

        const int imageHeaderSize = sizeof(uint32_t) * 2;
        const int totalTransferSize = image->GetSize() + imageHeaderSize;

        // Send the image header
        ret = m_usbDevice->Write(m_imageBuffer, imageHeaderSize, &transferred);
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to write image" << endLog;
            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
            return ret;
        }

        totalTransferred += transferred;

        m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_PROGRESS, ((double)totalTransferred / totalTransferSize) * 100, image->GetName());

        log(ASTRA_LOG_LEVEL_DEBUG) << "Total transfer size: " << totalTransferSize << endLog;
        log(ASTRA_LOG_LEVEL_DEBUG) << "Total transferred: " << totalTransferred << endLog;
        while (totalTransferred < totalTransferSize) {
            int dataBlockSize = image->GetDataBlock(m_imageBuffer, m_imageBufferSize);
            if (dataBlockSize < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get data block" << endLog;
                m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
                return -1;
            }

            ret = m_usbDevice->Write(m_imageBuffer, dataBlockSize, &transferred);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to write image" << endLog;
                m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
                return ret;
            }

            totalTransferred += transferred;

            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_PROGRESS, ((double)totalTransferred / totalTransferSize) * 100, image->GetName());
        }

        if (totalTransferred != totalTransferSize) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to transfer entire image" << endLog;
            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
            return -1;
        }

        ret = UpdateImageSizeRequestFile(image->GetSize());
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to update image size request file" << endLog;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_COMPLETE, 100, image->GetName());

        return 0;
    }

    int HandleImageRequests()
    {
        ASTRA_LOG;

        int ret = 0;

        while (true) {
            std::unique_lock<std::mutex> lock(m_imageRequestMutex);
            m_imageRequestCV.wait(lock);
            if (m_shutdown.load()) {
                return 0;
            }

            std::string imageNamePrefix;

            if (m_requestedImageName.find('/') != std::string::npos) {
                size_t pos = m_requestedImageName.find('/');
                imageNamePrefix = m_requestedImageName.substr(0, pos);
                m_requestedImageName = m_requestedImageName.substr(pos + 1);
                log(ASTRA_LOG_LEVEL_DEBUG) << "Requested image name prefix: '" << imageNamePrefix << "', requested Image Name: '" << m_requestedImageName << "'" << endLog;
            }

            auto it = std::find_if(m_images->begin(), m_images->end(), [this](const Image &img) {
                ASTRA_LOG;

                log(ASTRA_LOG_LEVEL_DEBUG) << "Comparing: " << img.GetName() << " to " << m_requestedImageName << endLog;
                for (char c : img.GetName()) {
                    log << std::hex << static_cast<int>(c) << " ";
                }
                log << " vs ";
                for (char c : m_requestedImageName) {
                    log << std::hex << static_cast<int>(c) << " ";
                }
                log << std::dec << endLog;
                return img.GetName() == m_requestedImageName;
            });

            Image *image;
            if (it == m_images->end()) {
                if (m_requestedImageName == m_sizeRequestImageFilename) {
                    image = m_sizeRequestImage.get();
                } else if (m_requestedImageName == m_usbPathImageFilename) {
                    image = m_usbPathImage.get();
                } else {
                    log(ASTRA_LOG_LEVEL_ERROR) << "Requested image not found: " << m_requestedImageName << endLog;
                    return -1;
                }
            } else {
                image = &(*it);
            }

            ret = SendImage(image);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to send image" << endLog;
                return ret;
            }
        }

        return 0;
    }

    void Shutdown() {
        if (!m_shutdown.load()) {
            m_shutdown.store(true);
            m_deviceEventCV.notify_all();

            m_imageRequestCV.notify_all();
        }
    }

};

AstraDevice::AstraDevice(std::unique_ptr<USBDevice> device) : 
    pImpl{std::make_unique<AstraDeviceImpl>(std::move(device))} {}

AstraDevice::~AstraDevice() = default;

void AstraDevice::SetStatusCallback(std::function<void(AstraDeviceState, double progress, std::string message)> statusCallback) {
    pImpl->SetStatusCallback(statusCallback);
}

int AstraDevice::Boot(std::shared_ptr<AstraBootFirmware> firmware) {
    return pImpl->Boot(firmware);
}

int AstraDevice::Update(std::shared_ptr<FlashImage> flashImage) {
    return pImpl->Update(flashImage);
}

int AstraDevice::WaitForCompletion() {
    return pImpl->WaitForCompletion();
}

int AstraDevice::SendToConsole(const std::string &data) {
    return pImpl->SendToConsole(data);
}

int AstraDevice::ReceiveFromConsole(std::string &data) {
    return pImpl->ReceiveFromConsole(data);
}