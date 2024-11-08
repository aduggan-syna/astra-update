#pragma once
#include <cstdint>
#include <cstddef>
#include <thread>
#include <atomic>
#include <functional>
#include <libusb-1.0/libusb.h>

#include "device.hpp"

class USBDevice : public Device {
public:
    USBDevice(libusb_device *device, libusb_context *ctx);
    ~USBDevice();

    int Open(std::function<void(uint8_t *buf, size_t size)> interruptCallback);
    void Close() override;

    int Read(uint8_t *data, size_t size, int *transferred) override;
    int Write(const uint8_t *data, size_t size, int *transferred) override;

private:
    libusb_device *m_device;
    libusb_context *m_ctx;
    libusb_device_handle *m_handle;
    libusb_config_descriptor *m_config;
    struct libusb_transfer *m_inputInterruptXfer;
    struct libusb_transfer *m_outputInterruptXfer;
    std::thread m_deviceThread;
    std::atomic<bool> m_running;
    bool m_open;

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

    std::function<void(uint8_t *buf, size_t size)> m_inputInterruptCallback;

    void DeviceThread();
    static void LIBUSB_CALL HandleInputInterruptTransfer(struct libusb_transfer *transfer);
};