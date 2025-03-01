#pragma once
#include <cstdint>
#include <cstddef>
#include <thread>
#include <atomic>
#include <functional>
#include <libusb-1.0/libusb.h>
#include <mutex>

#include "device.hpp"

class USBDevice : public Device {
public:
    USBDevice(libusb_device *device, libusb_context *ctx);
    ~USBDevice();

    enum USBEvent {
        USB_DEVICE_EVENT_NO_DEVICE,
        USB_DEVICE_EVENT_TRANSFER_CANCELED,
        USB_DEVICE_EVENT_INTERRUPT,
    };

    int Open(std::function<void(USBEvent event, uint8_t *buf, size_t size)> usbEventCallback);
    int EnableInterrupts();
    void Close() override;

    std::string &GetUSBPath() { return m_usbPath; }

    int Read(uint8_t *data, size_t size, int *transferred) override;
    int Write(const uint8_t *data, size_t size, int *transferred) override;

    int WriteInterruptData(const uint8_t *data, size_t size);

private:
    libusb_device *m_device;
    libusb_context *m_ctx;
    libusb_device_handle *m_handle;
    libusb_config_descriptor *m_config;
    struct libusb_transfer *m_inputInterruptXfer;
    struct libusb_transfer *m_outputInterruptXfer;
    std::atomic<bool> m_running;
    std::mutex m_closeMutex;
    std::string m_serialNumber;
    std::string m_usbPath;
    int m_interfaceNumber;

    uint8_t m_interruptInEndpoint;
    uint8_t m_interruptOutEndpoint;
    size_t m_interruptInSize;
    size_t m_interruptOutSize;
    uint8_t *m_interruptInBuffer;
    uint8_t *m_interruptOutBuffer;

    uint8_t m_bulkInEndpoint;
    uint8_t m_bulkOutEndpoint;
    size_t m_bulkInSize;
    size_t m_bulkOutSize;

    int m_bulkTransferTimeout;

    std::function<void(USBEvent event, uint8_t *buf, size_t size)> m_usbEventCallback;

    static void LIBUSB_CALL HandleInputInterruptTransfer(struct libusb_transfer *transfer);
};