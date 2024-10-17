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

#if 0
    std::vector<AstraBootImage> images = m_bootImages.GetSupportedImages(0x2bc5, 0x0401);

    if (images.empty()) {
        std::cerr << "No boot images found for the connected device" << std::endl;
        return;
    }

    for (const auto& image : images) {
        std::cout << "Found boot image: " << image.GetChipName() << " " << image.GetBoardName() << std::endl;
    }
#endif

    if (m_transport->Init() < 0) {
        return 1;
    }

    std::cout << "USB transport initialized successfully" << std::endl;

    m_transport->StartHotplugMonitoring();
    std::cout << "Hotplug monitoring started" << std::endl;
    
    m_transport->HandleHotplugEvents();

    return 0;
}