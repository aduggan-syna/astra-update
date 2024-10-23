#include <iostream>
#include "astra_update.hpp"
#include "boot_firmware_collection.hpp"
#include "usb_transport.hpp"

class AstraUpdate::AstraUpdateImpl {
public:
    AstraUpdateImpl(std::string flashImage, std::string bootFirmwarePath)
        : m_flashImage{flashImage}, m_bootFirmwarePath{bootFirmwarePath} {
        m_bootFirmwares = new BootFirmwareCollection{bootFirmwarePath};
        m_transport = new USBTransport{};
    }

    ~AstraUpdateImpl() {
        delete m_bootFirmwares;
        delete m_transport;
    }

    int Update() {
        m_bootFirmwares->Load();

        if (m_transport->Init() < 0) {
            return 1;
        }

        std::cout << "USB transport initialized successfully" << std::endl;

        std::vector<std::unique_ptr<USBDevice>> devices = m_transport->SearchForDevices(m_bootFirmwares->GetDeviceIDs());

        if (devices.empty()) {
            std::cerr << "No devices found" << std::endl;
            return 1;
        }

        for (const auto& device : devices) {
            if (!device->Open()) {
                std::cerr << "Failed to open device" << std::endl;
                return 1;
            }
        }

        return 0;
    }

private:
    std::string m_bootFirmwarePath;
    std::string m_flashImage;
    BootFirmwareCollection* m_bootFirmwares;
    USBTransport* m_transport;
};

AstraUpdate::AstraUpdate(std::string flashImage, std::string bootFirmwarePath)
    : pImpl{std::make_unique<AstraUpdateImpl>(flashImage, bootFirmwarePath)} {}

AstraUpdate::~AstraUpdate() = default;

int AstraUpdate::Update() {
    return pImpl->Update();
}