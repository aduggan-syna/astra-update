#pragma once
#include <vector>
#include <tuple>
#include <cstdint>
#include <memory>
#include <libusb-1.0/libusb.h>

#include "usb_device.hpp"

class USBTransport {
public:
    USBTransport() : m_ctx{nullptr}, m_deviceFound{false}
    {}
    ~USBTransport();

    int Init();
    
    std::vector<std::unique_ptr<USBDevice>> SearchForDevices(std::vector<std::tuple<uint16_t, uint16_t>> deviceIds);

protected:
    libusb_context *m_ctx;
    libusb_hotplug_callback_handle m_callbackHandle;
    std::vector<std::tuple<uint16_t, uint16_t>> m_deviceIds;
    std::vector<std::unique_ptr<USBDevice>> m_devices;
    bool m_deviceFound;

    void StartHotplugMonitoring();
    void StopHotplugMonitoring();

    static int LIBUSB_CALL HotplugEventCallback(libusb_context *ctx, libusb_device *device,
                                                libusb_hotplug_event event, void *user_data);
};