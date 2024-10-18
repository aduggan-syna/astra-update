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

std::vector<std::unique_ptr<USBDevice>> USBTransport::SearchForDevices(std::vector<std::tuple<uint16_t, uint16_t>> deviceIds)
{
    int ret;
    m_deviceIds = deviceIds;

    StartHotplugMonitoring();

    while (!m_deviceFound) {
        ret = libusb_handle_events(m_ctx);
        if (ret < 0) {
            std::cerr << "Failed to handle events: " << libusb_error_name(ret) << std::endl;
            break;
        }
    }

    StopHotplugMonitoring();

    return std::move(m_devices); // Use std::move to transfer ownership
}

void USBTransport::StartHotplugMonitoring() {
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        std::cerr << "Hotplug capability is not supported on this platform" << std::endl;
        return;
    }

    int ret = libusb_hotplug_register_callback(m_ctx,
                                             LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                             LIBUSB_HOTPLUG_ENUMERATE,
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
    USBTransport *transport = static_cast<USBTransport*>(user_data);

    libusb_device_descriptor desc;
    int ret = libusb_get_device_descriptor(device, &desc);
    if (ret < 0) {
        std::cerr << "Failed to get device descriptor" << std::endl;
        return 1;
    }

    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        std::cout << "Device arrived: vid: 0x" << std::hex << std::uppercase << desc.idVendor << ", pid: 0x" << desc.idProduct << std::endl;
        for (const auto& id : transport->m_deviceIds) {
            if (desc.idVendor == std::get<0>(id) && desc.idProduct == std::get<1>(id)) {
                std::cout << "Device matches image" << std::endl;
                transport->m_deviceFound = true;
                transport->m_devices.push_back(std::make_unique<USBDevice>(device));
                break;
            }
        }
    }
    return 0;
}