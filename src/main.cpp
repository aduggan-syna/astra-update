#include <iostream>
#include "usb_transport.hpp"

int main() {
    USBTransport transport;

    if (transport.init() < 0) {
        return 1;
    }

    std::cout << "USB transport initialized successfully" << std::endl;

    transport.start_hotplug_monitoring();
    std::cout << "Hotplug monitoring started" << std::endl;
    
    transport.handle_hotplug_events();

    return 0;
}