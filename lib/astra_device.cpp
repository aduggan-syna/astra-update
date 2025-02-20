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

class AstraDevice::AstraDeviceImpl {
public:
    AstraDeviceImpl(std::unique_ptr<USBDevice> device) : m_usbDevice{std::move(device)}
    {}

    ~AstraDeviceImpl() = default;

    void SetStatusCallback(std::function<void(AstraDeviceState, double progress, std::string message)> statusCallback) {
        m_statusCallback = statusCallback;
    }

    int Boot(std::shared_ptr<AstraBootFirmware> firmware) {
        int ret;

        m_ubootConsole = firmware->GetUbootConsole();
        m_uEnvSupport = firmware->GetUEnvSupport();

        ret = m_usbDevice->Open(std::bind(&AstraDeviceImpl::USBEventHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        if (ret < 0) {
            std::cerr << "Failed to open device" << std::endl;
            return ret;
        }

        m_tempDir = MakeTempDirerctory();
        if (m_tempDir.empty()) {
            std::cerr << "Failed to create temporary directory" << std::endl;
            return -1;
        }

        std::ofstream imageFile(m_tempDir + m_usbPathImageFilename);
        if (!imageFile) {
            std::cerr << "Failed to open 06_IMAGE file" << std::endl;
            return -1;
        }

        imageFile << m_usbDevice->GetUSBPath();
        imageFile.close();

        m_usbPathImage = std::make_unique<Image>(m_tempDir + m_usbPathImageFilename);
        m_sizeRequestImage = std::make_unique<Image>(m_tempDir + m_sizeRequestImageFilename);

        m_state = ASTRA_DEVICE_STATE_OPENED;

        std::vector<Image> firmwareImages = firmware->GetImages();
        m_images->insert(m_images->end(), firmwareImages.begin(), firmwareImages.end());

        m_imageRequestThread= std::thread(std::bind(&AstraDeviceImpl::ImageRequestThread, this));

        return 0;
    }

    int Update(std::shared_ptr<FlashImage> flashImage) {
        m_images->insert(m_images->end(), flashImage->GetImages().begin(), flashImage->GetImages().end());

        if (m_ubootConsole == ASTRA_UBOOT_CONSOLE_USB && !m_uEnvSupport) {
            if (m_console.WaitForPrompt()) {
                SendToConsole(flashImage->GetFlashCommand());
            }
        }

        return 0;
    }

    int WaitForCompletion() {
        if (m_uEnvSupport) {
            for (;;) {
                std::unique_lock<std::mutex> lock(m_deviceEventMutex);
                m_deviceEventCV.wait(lock);
                if (m_shutdown.load()) {
                    break;
                }
            }
        } else if (m_ubootConsole == ASTRA_UBOOT_CONSOLE_USB) {
            if (m_console.WaitForPrompt()) {
                SendToConsole("reset\n");
            }
        }

        return 0;
    }

    int SendToConsole(const std::string &data)
    {
        int ret = m_usbDevice->WriteInterruptData(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
        if (ret < 0) {
            std::cerr << "Failed to send data to console" << std::endl;
            return ret;
        }
        return 0;
    }

    int ReceiveFromConsole(std::string &data)
    {
        data = m_console.Get();
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

    AstraConsole m_console;
    enum AstraUbootConsole m_ubootConsole;

    std::string m_tempDir;
    const std::string m_usbPathImageFilename = "06_IMAGE";
    const std::string m_sizeRequestImageFilename = "07_IMAGE";
    std::unique_ptr<Image> m_usbPathImage;
    std::unique_ptr<Image> m_sizeRequestImage;

    void ImageRequestThread() {
        int ret = HandleImageRequests();
        if (ret < 0) {
            std::cerr << "Failed to handle image request" << std::endl;
        }
    }

    void HandleInterrupt(uint8_t *buf, size_t size) {
        std::cout << "Interrupt received: size:" << size << std::endl;

        for (size_t i = 0; i < size; ++i) {
            std::cout << std::hex << static_cast<int>(buf[i]) << " ";
        }
        std::cout << std::dec << std::endl;

        std::string message(reinterpret_cast<char *>(buf), size);
        std::cout << "Message: " << message << std::endl;

        auto it = message.find(m_imageRequestString);
        if (it != std::string::npos) {
            std::unique_lock<std::mutex> lock(m_imageRequestMutex);
            it += m_imageRequestString.size();
            m_imageType = buf[it];
            std::cout << "Image type: " << std::hex << m_imageType << std::dec << std::endl;
            std::string imageName = message.substr(it + 1);

            // Strip off Null character
            size_t end = imageName.find_last_not_of('\0');
            if (end != std::string::npos) {
                m_requestedImageName = imageName.substr(0, end + 1);
            } else {
                m_requestedImageName = imageName;
            }
            std::cout << "Requested image name: '" << m_requestedImageName << "'" << std::endl;

            m_imageRequestCV.notify_one();
        } else {
            m_console.Append(message);
        }
    }

    void USBEventHandler(USBDevice::USBEvent event, uint8_t *buf, size_t size) {
        if (event == USBDevice::USB_DEVICE_EVENT_INTERRUPT) {
            HandleInterrupt(buf, size);
        } else if (event == USBDevice::USB_DEVICE_EVENT_NO_DEVICE) {
            // device disappeared
            Shutdown();
        }
    }

    int UpdateImageSizeRequestFile(uint32_t fileSize)
    {
        if (m_imageType > 0x79)
        {
            std::string imageName = m_tempDir + m_requestedImageName;
            FILE *sizeFile = fopen(imageName.c_str(), "w");
            if (!sizeFile) {
                std::cerr << "Failed to open " << m_tempDir + m_sizeRequestImageFilename << " file" << std::endl;
                return -1;
            }
            std::cout << "Writing image size to 07_IMAGE: " << fileSize << std::endl;

            if (fwrite(&fileSize, sizeof(fileSize), 1, sizeFile) != 1) {
                std::cerr << "Failed to write image size to file" << std::endl;
                fclose(sizeFile);
                return -1;
            }
            fclose(sizeFile);
        }

        return 0;
    }

    int SendImage(Image *image)
    {
        int ret;
        int totalTransferred = 0;
        int transferred = 0;

        ret = image->Load();
        if (ret < 0) {
            std::cerr << "Failed to load image" << std::endl;
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
            std::cerr << "Failed to write image" << std::endl;
            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
            return ret;
        }

        totalTransferred += transferred;

        m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_PROGRESS, ((double)totalTransferred / totalTransferSize) * 100, image->GetName());

        std::cout << "Total transfer size: " << totalTransferSize << std::endl;
        std::cout << "Total transferred: " << totalTransferred << std::endl;
        while (totalTransferred < totalTransferSize) {
            int dataBlockSize = image->GetDataBlock(m_imageBuffer, m_imageBufferSize);
            if (dataBlockSize < 0) {
                std::cerr << "Failed to get data block" << std::endl;
                m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
                return -1;
            }

            ret = m_usbDevice->Write(m_imageBuffer, dataBlockSize, &transferred);
            if (ret < 0) {
                std::cerr << "Failed to write image" << std::endl;
                m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
                return ret;
            }

            totalTransferred += transferred;

            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_PROGRESS, ((double)totalTransferred / totalTransferSize) * 100, image->GetName());
        }

        if (totalTransferred != totalTransferSize) {
            std::cerr << "Failed to transfer entire image" << std::endl;
            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
            return -1;
        }

        ret = UpdateImageSizeRequestFile(image->GetSize());
        if (ret < 0) {
            std::cerr << "Failed to update image size request file" << std::endl;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_COMPLETE, 100, image->GetName());

        return 0;
    }

    int HandleImageRequests()
    {
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
                std::cout << "Requested image name prefix: '" << imageNamePrefix << "', requested Image Name: '" << m_requestedImageName << "'" << std::endl;
            }

            auto it = std::find_if(m_images->begin(), m_images->end(), [this](const Image &img) {
                std::cout << "Comparing: " << img.GetName() << " to " << m_requestedImageName << std::endl;
                for (char c : img.GetName()) {
                    std::cout << std::hex << static_cast<int>(c) << " ";
                }
                std::cout << " vs ";
                for (char c : m_requestedImageName) {
                    std::cout << std::hex << static_cast<int>(c) << " ";
                }
                std::cout << std::dec << std::endl;
                return img.GetName() == m_requestedImageName;
            });

            Image *image;
            if (it == m_images->end()) {
                if (m_requestedImageName == m_sizeRequestImageFilename) {
                    image = m_sizeRequestImage.get();
                } else if (m_requestedImageName == m_usbPathImageFilename) {
                    image = m_usbPathImage.get();
                } else {
                    std::cerr << "Requested image not found: " << m_requestedImageName << std::endl;
                    return -1;
                }
            } else {
                image = &(*it);
            }

            ret = SendImage(image);
            if (ret < 0) {
                std::cerr << "Failed to send image" << std::endl;
                return ret;
            }
        }

        return 0;
    }

    void Shutdown() {
        m_shutdown.store(true);
        m_deviceEventCV.notify_all();
        m_imageRequestCV.notify_all();
        m_console.Shutdown();
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