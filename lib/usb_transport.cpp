#include <iostream>
#include <usb_transport.hpp>
#include <libusb-1.0/libusb.h>

USBTransport::~USBTransport() {
    if (m_ctx) {
        libusb_exit(m_ctx);
    }
}

int USBTransport::init() {
    int ret = libusb_init(&m_ctx);
    if (ret < 0) {
        std::cerr << "Failed to initialize libusb: " << libusb_error_name(ret) << std::endl;
    }

    if (!libusb_has_capability (LIBUSB_CAP_HAS_HOTPLUG)) {
		std::cerr << "Hotplug capabilites are not supported on this platform" << std::endl;
	}

    return ret;
}

void USBTransport::start_hotplug_monitoring() {
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        std::cerr << "Hotplug capability is not supported on this platform" << std::endl;
        return;
    }

    int r = libusb_hotplug_register_callback(m_ctx,
                                             LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                             LIBUSB_HOTPLUG_NO_FLAGS,
                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                             hotplug_callback,
                                             this,
                                             &callback_handle);
    if (r != LIBUSB_SUCCESS) {
        std::cerr << "Failed to register hotplug callback: " << libusb_error_name(r) << std::endl;
    }
}

void USBTransport::stop_hotplug_monitoring() {
    if (callback_handle) {
        libusb_hotplug_deregister_callback(m_ctx, callback_handle);
        callback_handle = 0;
    }
}

int LIBUSB_CALL USBTransport::hotplug_callback(libusb_context *ctx, libusb_device *device,
                                                libusb_hotplug_event event, void *user_data) {
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        libusb_device_descriptor desc;
        int ret = libusb_get_device_descriptor(device, &desc);
        if (ret < 0) {
            std::cerr << "Failed to get device descriptor" << std::endl;
        } else {
            std::cout << "Device arrived: vid: " << desc.idVendor << ", pid: " << desc.idProduct << std::endl;
        }
    }
    return 0;
}

void USBTransport::handle_hotplug_events()
{
    while (true) {
        libusb_handle_events_completed(m_ctx, nullptr);
    }
}