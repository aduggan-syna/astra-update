#pragma once

#include <libusb-1.0/libusb.h>

class USBTransport {
public:
    USBTransport() : m_ctx(nullptr)
    {}
    ~USBTransport();

    int init();
    
    void start_hotplug_monitoring();
    void stop_hotplug_monitoring();

    void handle_hotplug_events();

protected:
	libusb_context *m_ctx;
    libusb_hotplug_callback_handle callback_handle;


    static int LIBUSB_CALL hotplug_callback(libusb_context *ctx, libusb_device *device,
                                                libusb_hotplug_event event, void *user_data);
};