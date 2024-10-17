#pragma once

#include <libusb-1.0/libusb.h>

class USBTransport {
public:
    USBTransport() : m_ctx{nullptr}
    {}
    ~USBTransport();

    int Init();
    
    void StartHotplugMonitoring();
    void StopHotplugMonitoring();

    void HandleHotplugEvents();

protected:
    libusb_context *m_ctx;
    libusb_hotplug_callback_handle m_callbackHandle;

    static int LIBUSB_CALL HotplugEventCallback(libusb_context *ctx, libusb_device *device,
                                                libusb_hotplug_event event, void *user_data);
};