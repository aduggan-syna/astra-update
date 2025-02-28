#include <iostream>
#include <cstddef>
#include <iomanip>
#include <cstring>

#include "usb_device.hpp"
#include "astra_log.hpp"

USBDevice::USBDevice(libusb_device *device, libusb_context *ctx)
{
    ASTRA_LOG;

    m_device = libusb_ref_device(device);
    m_ctx = ctx;
    m_handle = nullptr;
    m_config = nullptr;
    m_running.store(false);
    m_interruptInEndpoint = 0;
    m_interruptOutEndpoint = 0;
    m_interruptInSize = 0;
    m_interruptOutSize = 0;
    m_interruptInBuffer = nullptr;
    m_interruptOutBuffer = nullptr;
    m_bulkInEndpoint = 0;
    m_bulkOutEndpoint = 0;
    m_bulkInSize = 0;
    m_bulkOutSize = 0;
    m_bulkTransferTimeout = 0;
}

USBDevice::~USBDevice()
{
    ASTRA_LOG;

    Close();
}

int USBDevice::Open(std::function<void(USBEvent event, uint8_t *buf, size_t size)> usbEventCallback)
{
    ASTRA_LOG;

    if (m_handle) {
        return 0;
    }

    if (!usbEventCallback) {
        return 1;
    }

    m_usbEventCallback = usbEventCallback;

    int ret = libusb_open(m_device, &m_handle);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to open USB device: " << libusb_error_name(ret) << endLog;
        return 1;
    }

    if (m_handle == nullptr) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to open USB device" << endLog;
        return 1;
    }

    ret = libusb_get_config_descriptor(libusb_get_device(m_handle), 0, &m_config);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get config descriptor: " << libusb_error_name(ret) << endLog;
        return 1;
    }

    log(ASTRA_LOG_LEVEL_INFO) << "Configuration Descriptor:" << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  bLength: " << static_cast<int>(m_config->bLength) << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  bDescriptorType: " << static_cast<int>(m_config->bDescriptorType) << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  wTotalLength: " << m_config->wTotalLength << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  bNumInterfaces: " << static_cast<int>(m_config->bNumInterfaces) << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  bConfigurationValue: " << static_cast<int>(m_config->bConfigurationValue) << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  iConfiguration: " << static_cast<int>(m_config->iConfiguration) << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  bmAttributes: " << static_cast<int>(m_config->bmAttributes) << endLog;
    log(ASTRA_LOG_LEVEL_INFO) << "  MaxPower: " << static_cast<int>(m_config->MaxPower) << endLog;

    unsigned char serialNumber[256];
    libusb_device_descriptor desc;
    ret = libusb_get_device_descriptor(m_device, &desc);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get device descriptor: " << libusb_error_name(ret) << endLog;
        return 1;
    }

    if (desc.iSerialNumber != 0) {
        ret = libusb_get_string_descriptor_ascii(m_handle, desc.iSerialNumber, serialNumber, sizeof(serialNumber));
        if (ret < 0) {
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get serial number: " << libusb_error_name(ret) << endLog;
        } else {
            m_serialNumber = std::string(serialNumber, serialNumber + ret);
            log(ASTRA_LOG_LEVEL_INFO) << "Serial number: " << m_serialNumber << endLog;
        }
    }

    uint8_t *portNumbers = new uint8_t[8];
    uint8_t bus = libusb_get_bus_number(libusb_get_device(m_handle));
    uint8_t port = libusb_get_port_number(libusb_get_device(m_handle));
    int numElementsInPath = libusb_get_port_numbers(libusb_get_device(m_handle), portNumbers, 8);
    std::stringstream usbPathStream;
    usbPathStream << static_cast<int>(bus) << "-";
    log(ASTRA_LOG_LEVEL_DEBUG) << "Number of Elements in Path: " << numElementsInPath << endLog;
    if (numElementsInPath > 0) {
        usbPathStream << static_cast<int>(portNumbers[0]);
        for (int i = 1; i < numElementsInPath; ++i) {
            usbPathStream << "." << static_cast<int>(portNumbers[i]);
        }
    }
    delete[] portNumbers;
    m_usbPath = usbPathStream.str();
    log(ASTRA_LOG_LEVEL_DEBUG) << "USB Path: " << usbPathStream.str() << endLog;

    ret = libusb_detach_kernel_driver(m_handle, 0);
    if (ret < 0) {
        if (ret == LIBUSB_ERROR_NOT_FOUND || ret == LIBUSB_ERROR_NOT_SUPPORTED) {
            // Since some platforms don't support kernel driver detaching, we'll just log the error and continue
            log(ASTRA_LOG_LEVEL_INFO) << "Failed to detach kernel driver: " << libusb_error_name(ret) << endLog;
        } else {
            // If the error is something else, we'll return an error
            log(ASTRA_LOG_LEVEL_ERROR) << "Failed to detach kernel driver: " << libusb_error_name(ret) << endLog;
            return 1;
        }
    }

    ret = libusb_claim_interface(m_handle, 0);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to claim interface: " << libusb_error_name(ret) << endLog;
        return 1;
    }

    for (int i = 0; i < m_config->bNumInterfaces; ++i) {
        const libusb_interface &interface = m_config->interface[i];
        for (int j = 0; j < interface.num_altsetting; ++j) {
            const libusb_interface_descriptor &altsetting = interface.altsetting[j];
            log(ASTRA_LOG_LEVEL_INFO) << "Interface Descriptor:" << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bLength: " << static_cast<int>(altsetting.bLength) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bDescriptorType: " << static_cast<int>(altsetting.bDescriptorType) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bInterfaceNumber: " << static_cast<int>(altsetting.bInterfaceNumber) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bAlternateSetting: " << static_cast<int>(altsetting.bAlternateSetting) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bNumEndpoints: " << static_cast<int>(altsetting.bNumEndpoints) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bInterfaceClass: " << static_cast<int>(altsetting.bInterfaceClass) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bInterfaceSubClass: " << static_cast<int>(altsetting.bInterfaceSubClass) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  bInterfaceProtocol: " << static_cast<int>(altsetting.bInterfaceProtocol) << endLog;
            log(ASTRA_LOG_LEVEL_INFO) << "  iInterface: " << static_cast<int>(altsetting.iInterface) << endLog;

            for (int k = 0; k < altsetting.bNumEndpoints; ++k) {
                const libusb_endpoint_descriptor &endpoint = altsetting.endpoint[k];
                log(ASTRA_LOG_LEVEL_INFO) << "Endpoint Descriptor:" << endLog;
                log(ASTRA_LOG_LEVEL_INFO) << "  bLength: " << static_cast<int>(endpoint.bLength) << endLog;
                log(ASTRA_LOG_LEVEL_INFO) << "  bDescriptorType: " << static_cast<int>(endpoint.bDescriptorType) << endLog;
                log(ASTRA_LOG_LEVEL_INFO) << "  bEndpointAddress: " << static_cast<int>(endpoint.bEndpointAddress) << endLog;
                log(ASTRA_LOG_LEVEL_INFO) << "  bmAttributes: " << static_cast<int>(endpoint.bmAttributes) << endLog;
                log(ASTRA_LOG_LEVEL_INFO) << "  wMaxPacketSize: " << endpoint.wMaxPacketSize << endLog;
                log(ASTRA_LOG_LEVEL_INFO) << "  bInterval: " << static_cast<int>(endpoint.bInterval) << endLog;

                if (endpoint.bEndpointAddress & 0x80) {
                    if (endpoint.bmAttributes == 3) {
                        m_interruptInSize = endpoint.wMaxPacketSize;
                        m_interruptInEndpoint = endpoint.bEndpointAddress;
                    } else if (endpoint.bmAttributes == 2) {
                        m_bulkInSize = endpoint.wMaxPacketSize;
                        m_bulkInEndpoint = endpoint.bEndpointAddress;
                    }
                } else {
                    if (endpoint.bmAttributes == 3) {
                        m_interruptOutSize = endpoint.wMaxPacketSize;
                        m_interruptOutEndpoint = endpoint.bEndpointAddress;
                    } else if (endpoint.bmAttributes == 2) {
                        m_bulkOutSize = endpoint.wMaxPacketSize;
                        m_bulkOutEndpoint = endpoint.bEndpointAddress;
                    }
                }

                ret = libusb_clear_halt(m_handle, endpoint.bEndpointAddress);
                if (ret < 0) {
                    log(ASTRA_LOG_LEVEL_ERROR) << "Failed to clear halt on endpoint: " << libusb_error_name(ret) << endLog;
                    return 1;
                }
            }
        }
    }

    m_inputInterruptXfer = libusb_alloc_transfer(0);
    if (!m_inputInterruptXfer) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to allocate input interrupt transfer" << endLog;
        return 1;
    }
    m_outputInterruptXfer = libusb_alloc_transfer(0);
    if (!m_outputInterruptXfer) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to allocate output interrupt transfer" << endLog;
        return 1;
    }

    m_interruptInBuffer = new uint8_t[m_interruptInSize];
    m_interruptOutBuffer = new uint8_t[m_interruptOutSize];

    libusb_fill_interrupt_transfer(m_inputInterruptXfer, m_handle, m_interruptInEndpoint,
        m_interruptInBuffer, m_interruptInSize, HandleInputInterruptTransfer, this, 0);

    m_deviceThread = std::thread(&USBDevice::DeviceThread, this);

    ret = libusb_submit_transfer(m_inputInterruptXfer);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to submit input interrupt transfer: " << libusb_error_name(ret) << endLog;
    }

    m_running.store(true);

    return 0;
}

void USBDevice::Close()
{
    ASTRA_LOG;

    std::lock_guard<std::mutex> lock(m_closeMutex);
    if (m_running.exchange(false))
    {
        libusb_interrupt_event_handler(m_ctx);
        if (m_deviceThread.joinable()) {
            m_deviceThread.join();
        }

        if (m_inputInterruptXfer) {
            libusb_cancel_transfer(m_inputInterruptXfer);
            libusb_free_transfer(m_inputInterruptXfer);
            m_inputInterruptXfer = nullptr;
        }

        if (m_outputInterruptXfer) {
            libusb_cancel_transfer(m_outputInterruptXfer);
            libusb_free_transfer(m_outputInterruptXfer);
            m_outputInterruptXfer = nullptr;
        }

        delete[] m_interruptInBuffer;
        m_interruptInBuffer = nullptr;

        delete[] m_interruptOutBuffer;
        m_interruptOutBuffer = nullptr;

        if (m_handle) {
            libusb_close(m_handle);
            m_handle = nullptr;
        }

        libusb_unref_device(m_device);
    }
}

int USBDevice::Read(uint8_t *data, size_t size, int *transferred)
{
    ASTRA_LOG;

    if (!m_running.load()) {
        return 1;
    }

    log(ASTRA_LOG_LEVEL_DEBUG) << "Reading from USB device" << endLog;
    log(ASTRA_LOG_LEVEL_DEBUG) << "  Bulk In Endpoint: " << static_cast<int>(m_bulkInEndpoint) << endLog;
    int ret = libusb_bulk_transfer(m_handle, m_bulkInEndpoint, data, size, transferred, m_bulkTransferTimeout);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to read from USB device: " << libusb_error_name(ret) << endLog;
        return 1;
    }

    return 0;
}

int USBDevice::Write(const uint8_t *data, size_t size, int *transferred)
{
    ASTRA_LOG;

    if (!m_running.load()) {
        return 1;
    }

    log(ASTRA_LOG_LEVEL_DEBUG) << "Writing to USB device" << endLog;
    log(ASTRA_LOG_LEVEL_DEBUG) << "  Bulk Out Endpoint: " << static_cast<int>(m_bulkOutEndpoint) << endLog;
    log(ASTRA_LOG_LEVEL_DEBUG) << "  Length: " << size << endLog;
    log(ASTRA_LOG_LEVEL_DEBUG) << "  Data: ";
    for (size_t i = 0; i < 16 && i < size; ++i) {
        log << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    log << std::dec << endLog;

    int ret = libusb_bulk_transfer(m_handle, m_bulkOutEndpoint, const_cast<uint8_t*>(data), size, transferred, m_bulkTransferTimeout);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to write to USB device: " << libusb_error_name(ret) << endLog;
        return 1;
    }

    return 0;
}

int USBDevice::WriteInterruptData(const uint8_t *data, size_t size)
{
    ASTRA_LOG;

    if (!m_running.load()) {
        return 1;
    }

    log(ASTRA_LOG_LEVEL_DEBUG) << "Sending interrupt out transfer" << endLog;
    log(ASTRA_LOG_LEVEL_DEBUG) << "  Interrupt Out Endpoint: " << static_cast<int>(m_interruptOutEndpoint) << endLog;
    log(ASTRA_LOG_LEVEL_DEBUG) << "  Length: " << size << endLog;
    log(ASTRA_LOG_LEVEL_DEBUG) << "  Data: ";
    for (size_t i = 0; i < 16 && i < size; ++i) {
        log << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    log << std::dec << endLog;

    std::memcpy(m_interruptOutBuffer, data, size);

    libusb_fill_interrupt_transfer(m_outputInterruptXfer, m_handle, m_interruptOutEndpoint,
        m_interruptOutBuffer, size, nullptr, nullptr, 0);

    int ret = libusb_submit_transfer(m_outputInterruptXfer);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to submit output interrupt transfer: " << libusb_error_name(ret) << endLog;
        return 1;
    }

    return 0;
}

void USBDevice::DeviceThread()
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

void USBDevice::HandleInputInterruptTransfer(struct libusb_transfer *transfer)
{
    ASTRA_LOG;

    USBDevice *device = static_cast<USBDevice*>(transfer->user_data);
    
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        device->m_usbEventCallback(USB_DEVICE_EVENT_INTERRUPT, transfer->buffer, transfer->actual_length);
    } else if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Device is no longer there during transfer: " << libusb_error_name(transfer->status) << endLog;
        device->m_usbEventCallback(USB_DEVICE_EVENT_NO_DEVICE, nullptr, 0);
    } else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        log(ASTRA_LOG_LEVEL_DEBUG) << "Input interrupt transfer cancelled" << endLog;
        device->m_usbEventCallback(USB_DEVICE_EVENT_TRANSFER_CANCELED, nullptr, 0);
    } else {
        log(ASTRA_LOG_LEVEL_ERROR) << "Input interrupt transfer failed: " << libusb_error_name(transfer->status) << endLog;
    }

    log(ASTRA_LOG_LEVEL_DEBUG) << "Resubmitting input interrupt transfer" << endLog;
    int ret = libusb_submit_transfer(transfer);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to submit input interrupt transfer: " << libusb_error_name(ret) << endLog;
    }
}