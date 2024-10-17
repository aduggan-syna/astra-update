#include <iostream>
#include <libusb-1.0/libusb.h>

#include "usb_transport.hpp"

USBTransport::~USBTransport() {
    if (m_ctx) {
        libusb_exit(m_ctx);
    }
}

int USBTransport::Init() {
    int ret = libusb_init(&m_ctx);
    if (ret < 0) {
        std::cerr << "Failed to initialize libusb: " << libusb_error_name(ret) << std::endl;
    }

    return ret;
}

void USBTransport::StartHotplugMonitoring() {
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        std::cerr << "Hotplug capability is not supported on this platform" << std::endl;
        return;
    }

    int ret = libusb_hotplug_register_callback(m_ctx,
                                             LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                             LIBUSB_HOTPLUG_NO_FLAGS,
                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                             HotplugEventCallback,
                                             this,
                                             &m_callbackHandle);
    if (ret != LIBUSB_SUCCESS) {
        std::cerr << "Failed to register hotplug callback: " << libusb_error_name(ret) << std::endl;
    }
}

void USBTransport::StopHotplugMonitoring() {
    if (m_callbackHandle) {
        libusb_hotplug_deregister_callback(m_ctx, m_callbackHandle);
        m_callbackHandle = 0;
    }
}

int LIBUSB_CALL USBTransport::HotplugEventCallback(libusb_context *ctx, libusb_device *device,
                                                libusb_hotplug_event event, void *user_data)
{

    libusb_device_descriptor desc;
    int ret = libusb_get_device_descriptor(device, &desc);
    if (ret < 0) {
        std::cerr << "Failed to get device descriptor" << std::endl;
    } else {
        if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
            std::cout << "Device arrived: vid: 0x" << std::hex << std::uppercase << desc.idVendor << ", pid: 0x" << desc.idProduct << std::endl;
        } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
            std::cout << "Device left: vid: 0x" << std::hex << std::uppercase << desc.idVendor << ", pid: 0x" << desc.idProduct << std::endl;
        }
    }
    return 0;
}

void USBTransport::HandleHotplugEvents()
{
    while (true) {
        libusb_handle_events_completed(m_ctx, nullptr);
    }
}