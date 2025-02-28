#include <iostream>
#include <iomanip>
#include <libusb-1.0/libusb.h>
#include <thread>
#include <atomic>
#include <chrono>

#include "usb_transport.hpp"
#include "astra_log.hpp"

USBTransport::~USBTransport()
{
    ASTRA_LOG;
    Shutdown();
}

void USBTransport::DeviceMonitorThread()
{
    ASTRA_LOG;

    int ret;

    while (m_running.load()) {
        ret = libusb_handle_events(m_ctx);
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to handle events: " << libusb_error_name(ret) << endLog;
            break;
        }
    }
}

void USBTransport::DevicePollingThread()
{
    ASTRA_LOG;

    while (m_running.load()) {
        // Poll for device changes
        libusb_device **device_list;
        ssize_t count = libusb_get_device_list(m_ctx, &device_list);
        if (count < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get device list: " << libusb_error_name(count) << endLog;
            continue;
        }

        for (ssize_t i = 0; i < count; ++i) {
            libusb_device *device = device_list[i];
            libusb_device_descriptor desc;
            int ret = libusb_get_device_descriptor(device, &desc);
            if (ret < 0) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get device descriptor" << endLog;
                continue;
            }

            // Check if the device matches the vendorId and productId
            if (desc.idVendor == m_vendorId && desc.idProduct == m_productId) {
                std::unique_ptr<USBDevice> usbDevice = std::make_unique<USBDevice>(device, m_ctx);
                if (m_deviceAddedCallback) {
                    try {
                        m_deviceAddedCallback(std::move(usbDevice));
                    } catch (const std::bad_function_call& e) {
                        log(ASTRA_LOG_LEVEL_ERROR) << "Error: " << e.what() << endLog;
                    }
                } else {
                    log(ASTRA_LOG_LEVEL_ERROR) << "No device added callback" << endLog;
                }
            }
        }

        libusb_free_device_list(device_list, 1);

        // Sleep for a while before polling again
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int USBTransport::Init(uint16_t vendorId, uint16_t productId, std::function<void(std::unique_ptr<USBDevice>)> deviceAddedCallback)
{
    ASTRA_LOG;

    m_vendorId = vendorId;
    m_productId = productId;

    int ret = libusb_init(&m_ctx);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to initialize libusb: " << libusb_error_name(ret) << endLog;
    }

    //libusb_set_option(m_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);

    m_deviceAddedCallback = deviceAddedCallback;

    m_running.store(true);
    if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        log(ASTRA_LOG_LEVEL_DEBUG) << "Hotplug is supported" << endLog;

        ret = libusb_hotplug_register_callback(m_ctx,
                                                LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                                LIBUSB_HOTPLUG_ENUMERATE,
                                                vendorId,
                                                productId,
                                                LIBUSB_HOTPLUG_MATCH_ANY,
                                                HotplugEventCallback,
                                                this,
                                                &m_callbackHandle);
        if (ret != LIBUSB_SUCCESS) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to register hotplug callback: " << libusb_error_name(ret) << endLog;
        }

    } else {
        log(ASTRA_LOG_LEVEL_DEBUG) << "Hotplug is NOT supported" << endLog;

        m_deviceMonitorThread = std::thread(&USBTransport::DevicePollingThread, this);
    }

    m_deviceMonitorThread = std::thread(&USBTransport::DeviceMonitorThread, this);

    return ret;
}

void USBTransport::Shutdown()
{
    ASTRA_LOG;

    std::lock_guard<std::mutex> lock(m_shutdownMutex);
    if (m_running.exchange(false)) {
        if (m_callbackHandle) {
            libusb_hotplug_deregister_callback(m_ctx, m_callbackHandle);
            m_callbackHandle = 0;
        }

        libusb_interrupt_event_handler(m_ctx);
        if (m_deviceMonitorThread.joinable()) {
            m_deviceMonitorThread.join();
        }

        if (m_ctx) {
            libusb_exit(m_ctx);
        }
    }
}

int LIBUSB_CALL USBTransport::HotplugEventCallback(libusb_context *ctx, libusb_device *device,
                                                libusb_hotplug_event event, void *user_data)
{
    ASTRA_LOG;

    USBTransport *transport = static_cast<USBTransport*>(user_data);

    libusb_device_descriptor desc;
    int ret = libusb_get_device_descriptor(device, &desc);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get device descriptor" << endLog;
        return 1;
    }

    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        log(ASTRA_LOG_LEVEL_INFO) << "Device arrived: vid: 0x" << std::hex << std::uppercase << desc.idVendor << ", pid: 0x" << desc.idProduct << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "Device matches image" << endLog;

        log(ASTRA_LOG_LEVEL_INFO) << "Device Descriptor:" << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bLength: " << static_cast<int>(desc.bLength) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bDescriptorType: " << static_cast<int>(desc.bDescriptorType) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bcdUSB: " << desc.bcdUSB << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bDeviceClass: " << static_cast<int>(desc.bDeviceClass) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bDeviceSubClass: " << static_cast<int>(desc.bDeviceSubClass) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bDeviceProtocol: " << static_cast<int>(desc.bDeviceProtocol) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bMaxPacketSize0: " << static_cast<int>(desc.bMaxPacketSize0) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  idVendor: 0x" << std::hex << std::setw(4) << std::setfill('0') << desc.idVendor << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  idProduct: 0x" << std::hex << std::setw(4) << std::setfill('0') << desc.idProduct << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bcdDevice: " << desc.bcdDevice << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  iManufacturer: " << static_cast<int>(desc.iManufacturer) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  iProduct: " << static_cast<int>(desc.iProduct) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  iSerialNumber: " << static_cast<int>(desc.iSerialNumber) << endLog;
        log(ASTRA_LOG_LEVEL_INFO) << "  bNumConfigurations: " << static_cast<int>(desc.bNumConfigurations) << endLog;

        std::unique_ptr<USBDevice> usbDevice = std::make_unique<USBDevice>(device, transport->m_ctx);
        if (transport->m_deviceAddedCallback) {
            try {
                transport->m_deviceAddedCallback(std::move(usbDevice));
            } catch (const std::bad_function_call& e) {
                log(ASTRA_LOG_LEVEL_ERROR) << "Error: " << e.what() << endLog;
                return 1;
            }
        } else {
            log(ASTRA_LOG_LEVEL_ERROR) << "No device added callback" << endLog;
        }
    }

    return 0;
}