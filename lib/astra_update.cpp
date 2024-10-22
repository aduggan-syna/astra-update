#include <iostream>
#include "astra_update.hpp"
#include "boot_firmware_collection.hpp"
#include "usb_transport.hpp"

AstraUpdate::AstraUpdate(std::string bootFirmwarePath, std::string osImage) : m_bootFirmwarePath{bootFirmwarePath}, m_osImage{osImage}
{
    m_bootFirmwares = new BootFirmwareCollection{bootFirmwarePath};
    m_transport = new USBTransport{};
}

AstraUpdate::~AstraUpdate()
{
    delete m_bootFirmwares;
    delete m_transport;
}

int AstraUpdate::Run()
{
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