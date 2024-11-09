#include <iostream>
#include <memory>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <cstring>

#include "astra_device.hpp"
#include "astra_boot_firmware.hpp"
#include "flash_image.hpp"
#include "console.hpp"
#include "usb_device.hpp"
#include "image.hpp"

class AstraDevice::AstraDeviceImpl {
public:
    AstraDeviceImpl(std::unique_ptr<USBDevice> device) : m_usbDevice{std::move(device)}
    {}

    ~AstraDeviceImpl() {
        std::cout << "Console: " << m_console.Get() << std::endl;
    }

    void SetStatusCallback(std::function<void(AstraDeviceState, int progress, std::string message)> statusCallback) {
        m_statusCallback = statusCallback;
    }

    int Open() {
        int ret;

        ret = m_usbDevice->Open(std::bind(&AstraDeviceImpl::HandleInterrupt, this, std::placeholders::_1, std::placeholders::_2));
        if (ret < 0) {
            std::cerr << "Failed to open device" << std::endl;
            return ret;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_OPENED, 0, "Device opened");

        return 0;
    }

    int Boot(AstraBootFirmware &firmware) {
        int ret;

        std::vector<Image> images = firmware.GetImages();

        m_statusCallback(ASTRA_DEVICE_STATE_BOOT_START, 0, "Booting device");

        ret = HandleImageRequests(images);
        if (ret < 0) {
            std::cerr << "Failed to handle image request" << std::endl;
            return ret;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_BOOT_COMPLETE, 100, "Device booted");

        return 0;
    }

    int Update(std::shared_ptr<FlashImage> image) {
        int ret;

        m_statusCallback(ASTRA_DEVICE_STATE_UPDATE_START, 0, "Updating device");

        m_statusCallback(ASTRA_DEVICE_STATE_UPDATE_COMPLETE, 100, "Device updated");

        return 0;
    }

    int Reset() {
        return 0;
    }

private:
    std::unique_ptr<USBDevice> m_usbDevice;
    AstraDeviceState m_state;
    std::function<void(AstraDeviceState, int progress, std::string message)> m_statusCallback;

    std::condition_variable m_imageRequestCV;
    std::mutex m_imageRequestMutex;
    uint8_t m_imageType;
    std::string m_requestedImageName;

    const std::string m_imageRequestString = "i*m*g*r*q*";
    static constexpr int m_imageBufferSize = (1 * 1024 * 1024) + 4;
    uint8_t m_imageBuffer[m_imageBufferSize];

    Console m_console;

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

        m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_PROGRESS, totalTransferred / totalTransferSize, image->GetName());

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

            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_PROGRESS, totalTransferred / totalTransferSize, image->GetName());
        }

        if (totalTransferred != totalTransferSize) {
            std::cerr << "Failed to transfer entire image" << std::endl;
            m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_FAIL, 0, image->GetName());
            return -1;
        }

        m_statusCallback(ASTRA_DEVICE_STATE_IMAGE_SEND_COMPLETE, 100, image->GetName());

        return 0;
    }

    int HandleImageRequests(std::vector<Image> images)
    {
        int ret = 0;

        while (true) {
            std::unique_lock<std::mutex> lock(m_imageRequestMutex);
            m_imageRequestCV.wait(lock);

            auto it = std::find_if(images.begin(), images.end(), [this](const Image &img) {
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

            if (it == images.end()) {
                std::cerr << "Requested image not found: " << m_requestedImageName << std::endl;
                return -1;
            }

            Image *image = &(*it);
            ret = SendImage(image);
            if (ret < 0) {
                std::cerr << "Failed to send image" << std::endl;
                return ret;
            }
        }

        return 0;
    }

};

AstraDevice::AstraDevice(std::unique_ptr<USBDevice> device) : 
    pImpl{std::make_unique<AstraDeviceImpl>(std::move(device))} {}

AstraDevice::~AstraDevice() = default;

void AstraDevice::SetStatusCallback(std::function<void(AstraDeviceState, int progress, std::string message)> statusCallback) {
    pImpl->SetStatusCallback(statusCallback);
}

int AstraDevice::Open() {
    return pImpl->Open();
}

int AstraDevice::Boot(AstraBootFirmware &firmware) {
    return pImpl->Boot(firmware);
}

int AstraDevice::Update(std::shared_ptr<FlashImage> image) {
    return pImpl->Update(image);
}

int AstraDevice::Reset() {
    return pImpl->Reset();
}