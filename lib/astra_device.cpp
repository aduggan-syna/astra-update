#include <iostream>
#include <memory>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <cstring>

#include "astra_device.hpp"
#include "astra_boot_firmware.hpp"
#include "flash_image.hpp"
#include "astra_console.hpp"
#include "usb_device.hpp"
#include "image.hpp"

class AstraDevice::AstraDeviceImpl {
public:
    AstraDeviceImpl(std::unique_ptr<USBDevice> device) : m_usbDevice{std::move(device)}
    {}

    ~AstraDeviceImpl() {
    }

    void SetStatusCallback(std::function<void(AstraDeviceState, double progress, std::string message)> statusCallback) {
        m_statusCallback = statusCallback;
    }

    int Open(std::shared_ptr<AstraBootFirmware> firmware, std::shared_ptr<FlashImage> image) {
        int ret;

        m_ubootConsole = firmware->GetUbootConsole();

        ret = m_usbDevice->Open(std::bind(&AstraDeviceImpl::HandleInterrupt, this, std::placeholders::_1, std::placeholders::_2));
        if (ret < 0) {
            std::cerr << "Failed to open device" << std::endl;
            return ret;
        }

        std::ofstream imageFile(m_usbPathImageFilename);
        if (!imageFile) {
            std::cerr << "Failed to open 06_IMAGE file" << std::endl;
            return -1;
        }

        imageFile << m_usbDevice->GetUSBPath();
        imageFile.close();

        m_state = ASTRA_DEVICE_STATE_OPENED;

        std::vector<Image> images = firmware->GetImages();
        images.insert(images.end(), image->GetImages().begin(), image->GetImages().end());

        ret = HandleImageRequests(images);
        if (ret < 0) {
            std::cerr << "Failed to handle image request" << std::endl;
            return ret;
        }

        return 0;
    }

    int Boot() {
        return 0;
    }

    int Update() {
        return 0;
    }

    int Reset() {
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

    std::condition_variable m_imageRequestCV;
    std::mutex m_imageRequestMutex;
    uint8_t m_imageType;
    std::string m_requestedImageName;

    const std::string m_imageRequestString = "i*m*g*r*q*";
    static constexpr int m_imageBufferSize = (1 * 1024 * 1024) + 4;
    uint8_t m_imageBuffer[m_imageBufferSize];

    AstraConsole m_console;
    enum AstraUbootConsole m_ubootConsole;

    const std::string m_usbPathImageFilename = "06_IMAGE";
    const std::string m_sizeRequestImageFilename = "07_IMAGE";
    Image m_usbPathImage = Image(m_usbPathImageFilename);
    Image m_sizeRequestImage = Image(m_sizeRequestImageFilename);

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

    int UpdateImageSizeRequestFile(uint32_t fileSize)
    {
        if (m_imageType > 0x79)
        {
            FILE *sizeFile = fopen(m_sizeRequestImageFilename.c_str(), "w");
            if (!sizeFile) {
                std::cerr << "Failed to open " << m_sizeRequestImageFilename << " file" << std::endl;
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

    int HandleImageRequests(std::vector<Image> images)
    {
        int ret = 0;

        while (true) {
            std::unique_lock<std::mutex> lock(m_imageRequestMutex);
            m_imageRequestCV.wait(lock);

            std::string imageNamePrefix;

            if (m_requestedImageName.find('/') != std::string::npos) {
                size_t pos = m_requestedImageName.find('/');
                imageNamePrefix = m_requestedImageName.substr(0, pos);
                m_requestedImageName = m_requestedImageName.substr(pos + 1);
                std::cout << "Requested image name prefix: '" << imageNamePrefix << "', requested Image Name: '" << m_requestedImageName << "'" << std::endl;
            }

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

            Image *image;
            if (it == images.end()) {
                if (m_requestedImageName == m_sizeRequestImageFilename) {
                    image = &m_sizeRequestImage;
                } else if (m_requestedImageName == m_usbPathImageFilename) {
                    image = &m_usbPathImage;
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

};

AstraDevice::AstraDevice(std::unique_ptr<USBDevice> device) : 
    pImpl{std::make_unique<AstraDeviceImpl>(std::move(device))} {}

AstraDevice::~AstraDevice() = default;

void AstraDevice::SetStatusCallback(std::function<void(AstraDeviceState, double progress, std::string message)> statusCallback) {
    pImpl->SetStatusCallback(statusCallback);
}

int AstraDevice::Open(std::shared_ptr<AstraBootFirmware> firmware, std::shared_ptr<FlashImage> image) {
    return pImpl->Open(firmware, image);
}

int AstraDevice::Boot() {
    return pImpl->Boot();
}

int AstraDevice::Update() {
    return pImpl->Update();
}

int AstraDevice::Reset() {
    return pImpl->Reset();
}

int AstraDevice::SendToConsole(const std::string &data) {
    return pImpl->SendToConsole(data);
}

int AstraDevice::ReceiveFromConsole(std::string &data) {
    return pImpl->ReceiveFromConsole(data);
}