#include <iostream>
#include <cstddef>
#include <iomanip>
#include <cstring>

#include "usb_device.hpp"

USBDevice::USBDevice(libusb_device *device, libusb_context *ctx)
{
    m_device = libusb_ref_device(device);
    m_ctx = ctx;
    m_handle = nullptr;
    m_config = nullptr;
    m_open = false;
    m_bulkTransferTimeout = 0;
}

USBDevice::~USBDevice()
{
    if (m_open) {
        Close();
    }

    libusb_unref_device(m_device);
}

int USBDevice::Open(std::function<void(uint8_t *buf, size_t size)> interruptCallback)
{
    if (m_handle) {
        return 0;
    }

    if (!interruptCallback) {
        return 1;
    }

    m_inputInterruptCallback = interruptCallback;

    int ret = libusb_open(m_device, &m_handle);
    if (ret < 0) {
        std::cerr << "Failed to open USB device: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    if (m_handle == nullptr) {
        std::cerr << "Failed to open USB device" << std::endl;
        return 1;
    }

    m_open = true;

    ret = libusb_get_config_descriptor(libusb_get_device(m_handle), 0, &m_config);
    if (ret < 0) {
        std::cerr << "Failed to get config descriptor: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    std::cout << "Configuration Descriptor:" << std::endl;
    std::cout << "  bLength: " << static_cast<int>(m_config->bLength) << std::endl;
    std::cout << "  bDescriptorType: " << static_cast<int>(m_config->bDescriptorType) << std::endl;
    std::cout << "  wTotalLength: " << m_config->wTotalLength << std::endl;
    std::cout << "  bNumInterfaces: " << static_cast<int>(m_config->bNumInterfaces) << std::endl;
    std::cout << "  bConfigurationValue: " << static_cast<int>(m_config->bConfigurationValue) << std::endl;
    std::cout << "  iConfiguration: " << static_cast<int>(m_config->iConfiguration) << std::endl;
    std::cout << "  bmAttributes: " << static_cast<int>(m_config->bmAttributes) << std::endl;
    std::cout << "  MaxPower: " << static_cast<int>(m_config->MaxPower) << std::endl;

    unsigned char serialNumber[256];
    libusb_device_descriptor desc;
    ret = libusb_get_device_descriptor(m_device, &desc);
    if (ret < 0) {
        std::cerr << "Failed to get device descriptor: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    if (desc.iSerialNumber != 0) {
        ret = libusb_get_string_descriptor_ascii(m_handle, desc.iSerialNumber, serialNumber, sizeof(serialNumber));
        if (ret < 0) {
            std::cerr << "Failed to get serial number: " << libusb_error_name(ret) << std::endl;
        } else {
            m_serialNumber = std::string(serialNumber, serialNumber + ret);
            std::cout << "Serial number: " << m_serialNumber << std::endl;
        }
    }

    uint8_t *portNumbers = new uint8_t[8];
    uint8_t bus = libusb_get_bus_number(libusb_get_device(m_handle));
    uint8_t port = libusb_get_port_number(libusb_get_device(m_handle));
    int numElementsInPath = libusb_get_port_numbers(libusb_get_device(m_handle), portNumbers, 8);
    std::stringstream usbPathStream;
    usbPathStream << static_cast<int>(bus) << "-";
    std::cout << "Number of Elements in Path: " << numElementsInPath << std::endl;
    if (numElementsInPath > 0) {
        usbPathStream << static_cast<int>(portNumbers[0]);
        for (int i = 1; i < numElementsInPath; ++i) {
            usbPathStream << "." << static_cast<int>(portNumbers[i]);
        }
    }
    delete[] portNumbers;
    m_usbPath = usbPathStream.str();
    std::cout << "USB Path: " << usbPathStream.str() << std::endl;

    ret = libusb_detach_kernel_driver(m_handle, 0);
    if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND) {
        std::cerr << "Failed to detach kernel driver: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    ret = libusb_claim_interface(m_handle, 0);
    if (ret < 0) {
        std::cerr << "Failed to claim interface: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    for (int i = 0; i < m_config->bNumInterfaces; ++i) {
        const libusb_interface &interface = m_config->interface[i];
        for (int j = 0; j < interface.num_altsetting; ++j) {
            const libusb_interface_descriptor &altsetting = interface.altsetting[j];
            std::cout << "Interface Descriptor:" << std::endl;
            std::cout << "  bLength: " << static_cast<int>(altsetting.bLength) << std::endl;
            std::cout << "  bDescriptorType: " << static_cast<int>(altsetting.bDescriptorType) << std::endl;
            std::cout << "  bInterfaceNumber: " << static_cast<int>(altsetting.bInterfaceNumber) << std::endl;
            std::cout << "  bAlternateSetting: " << static_cast<int>(altsetting.bAlternateSetting) << std::endl;
            std::cout << "  bNumEndpoints: " << static_cast<int>(altsetting.bNumEndpoints) << std::endl;
            std::cout << "  bInterfaceClass: " << static_cast<int>(altsetting.bInterfaceClass) << std::endl;
            std::cout << "  bInterfaceSubClass: " << static_cast<int>(altsetting.bInterfaceSubClass) << std::endl;
            std::cout << "  bInterfaceProtocol: " << static_cast<int>(altsetting.bInterfaceProtocol) << std::endl;
            std::cout << "  iInterface: " << static_cast<int>(altsetting.iInterface) << std::endl;

            for (int k = 0; k < altsetting.bNumEndpoints; ++k) {
                const libusb_endpoint_descriptor &endpoint = altsetting.endpoint[k];
                std::cout << "Endpoint Descriptor:" << std::endl;
                std::cout << "  bLength: " << static_cast<int>(endpoint.bLength) << std::endl;
                std::cout << "  bDescriptorType: " << static_cast<int>(endpoint.bDescriptorType) << std::endl;
                std::cout << "  bEndpointAddress: " << static_cast<int>(endpoint.bEndpointAddress) << std::endl;
                std::cout << "  bmAttributes: " << static_cast<int>(endpoint.bmAttributes) << std::endl;
                std::cout << "  wMaxPacketSize: " << endpoint.wMaxPacketSize << std::endl;
                std::cout << "  bInterval: " << static_cast<int>(endpoint.bInterval) << std::endl;

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
                    std::cerr << "Failed to clear halt on endpoint: " << libusb_error_name(ret) << std::endl;
                    return 1;
                }
            }
        }
    }

    m_inputInterruptXfer = libusb_alloc_transfer(0);
    if (!m_inputInterruptXfer) {
        std::cerr << "Failed to allocate input interrupt transfer" << std::endl;
        return 1;
    }
    m_outputInterruptXfer = libusb_alloc_transfer(0);
    if (!m_outputInterruptXfer) {
        std::cerr << "Failed to allocate output interrupt transfer" << std::endl;
        return 1;
    }

    m_interruptInBuffer = new uint8_t[m_interruptInSize];
    m_interruptOutBuffer = new uint8_t[m_interruptOutSize];

    libusb_fill_interrupt_transfer(m_inputInterruptXfer, m_handle, m_interruptInEndpoint,
        m_interruptInBuffer, m_interruptInSize, HandleInputInterruptTransfer, this, 0);

    m_deviceThread = std::thread(&USBDevice::DeviceThread, this);

    ret = libusb_submit_transfer(m_inputInterruptXfer);
    if (ret < 0) {
        std::cerr << "Failed to submit input interrupt transfer: " << libusb_error_name(ret) << std::endl;
    }

    m_running = true;

    return 0;
}

void USBDevice::Close()
{
    if (m_running) {

        m_running = false;
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
    }

    if (m_handle) {
        libusb_close(m_handle);
        m_handle = nullptr;
    }

    m_open = false;
}

int USBDevice::Read(uint8_t *data, size_t size, int *transferred)
{
    if (!m_open) {
        return 1;
    }

    std::cout << "Reading from USB device" << std::endl;
    std::cout << "  Bulk In Endpoint: " << static_cast<int>(m_bulkInEndpoint) << std::endl;
    int ret = libusb_bulk_transfer(m_handle, m_bulkInEndpoint, data, size, transferred, m_bulkTransferTimeout);
    if (ret < 0) {
        std::cerr << "Failed to read from USB device: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    return 0;
}

int USBDevice::Write(const uint8_t *data, size_t size, int *transferred)
{
    if (!m_open) {
        return 1;
    }

    std::cout << "Writing to USB device" << std::endl;
    std::cout << "  Bulk Out Endpoint: " << static_cast<int>(m_bulkOutEndpoint) << std::endl;
    std::cout << "  Length: " << size << std::endl;
    std::cout << "  Data: ";
    for (size_t i = 0; i < 16 && i < size; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;

    int ret = libusb_bulk_transfer(m_handle, m_bulkOutEndpoint, const_cast<uint8_t*>(data), size, transferred, m_bulkTransferTimeout);
    if (ret < 0) {
        std::cerr << "Failed to write to USB device: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    return 0;
}

int USBDevice::WriteInterruptData(const uint8_t *data, size_t size)
{
    if (!m_open) {
        return 1;
    }

    std::cout << "Sending interrupt out transfer" << std::endl;
    std::cout << "  Interrupt Out Endpoint: " << static_cast<int>(m_interruptOutEndpoint) << std::endl;
    std::cout << "  Length: " << size << std::endl;
    std::cout << "  Data: ";
    for (size_t i = 0; i < 16 && i < size; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;

    std::memcpy(m_interruptOutBuffer, data, size);

    libusb_fill_interrupt_transfer(m_outputInterruptXfer, m_handle, m_interruptOutEndpoint,
        m_interruptOutBuffer, size, nullptr, nullptr, 0);

    int ret = libusb_submit_transfer(m_outputInterruptXfer);
    if (ret < 0) {
        std::cerr << "Failed to submit output interrupt transfer: " << libusb_error_name(ret) << std::endl;
        return 1;
    }

    return 0;
}

void USBDevice::DeviceThread()
{
    int ret;

    while (m_running) {
        ret = libusb_handle_events(m_ctx);
        if (ret < 0) {
            std::cerr << "Failed to handle events: " << libusb_error_name(ret) << std::endl;
            break;
        }
    }
}

void USBDevice::HandleInputInterruptTransfer(struct libusb_transfer *transfer)
{
    USBDevice *device = static_cast<USBDevice*>(transfer->user_data);
    
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        device->m_inputInterruptCallback(transfer->buffer, transfer->actual_length);
    } else {
        std::cerr << "Input interrupt transfer failed: " << libusb_error_name(transfer->status) << std::endl;
    }

    std::cout << "Resubmitting input interrupt transfer" << std::endl;
    int ret = libusb_submit_transfer(transfer);
    if (ret < 0) {
        std::cerr << "Failed to submit input interrupt transfer: " << libusb_error_name(ret) << std::endl;
    }
}