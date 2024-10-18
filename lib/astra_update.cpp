#include <iostream>
#include "astra_update.hpp"
#include "boot_image_collection.hpp"
#include "usb_transport.hpp"

AstraUpdate::AstraUpdate(std::string bootImagePath, std::string firmwareImage)
{
    m_bootImages = new BootImageCollection{bootImagePath};
    m_transport = new USBTransport{};
}

AstraUpdate::~AstraUpdate()
{
    delete m_bootImages;
    delete m_transport;
}

int AstraUpdate::Run()
{
    m_bootImages->Load();

    if (m_transport->Init() < 0) {
        return 1;
    }

    std::cout << "USB transport initialized successfully" << std::endl;

    std::vector<std::unique_ptr<USBDevice>> devices = m_transport->SearchForDevices(m_bootImages->GetDeviceIDs());

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