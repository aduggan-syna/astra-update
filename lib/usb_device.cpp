#include <iostream>
#include <cstddef>

#include "usb_device.hpp"

USBDevice::USBDevice(libusb_device *device)
{
    m_device = libusb_ref_device(device);
    m_handle = nullptr;
    m_config = nullptr;
    m_open = false;
}

USBDevice::~USBDevice()
{
    if (m_open) {
        Close();
    }

    libusb_unref_device(m_device);
}

bool USBDevice::Open()
{
    if (m_handle) {
        return true;
    }

    int ret = libusb_open(m_device, &m_handle);
    if (ret < 0) {
        std::cerr << "Failed to open USB device: " << libusb_error_name(ret) << std::endl;
        return false;
    }

    m_open = true;

    ret = libusb_get_config_descriptor(libusb_get_device(m_handle), 0, &m_config);
    if (ret < 0) {
        std::cerr << "Failed to get config descriptor: " << libusb_error_name(ret) << std::endl;
        return false;
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

    ret = libusb_detach_kernel_driver(m_handle, 0);
    if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND) {
        std::cerr << "Failed to detach kernel driver: " << libusb_error_name(ret) << std::endl;
        return false;
    }

    ret = libusb_claim_interface(m_handle, 0);
    if (ret < 0) {
        std::cerr << "Failed to claim interface: " << libusb_error_name(ret) << std::endl;
        return false;
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
                    }
                } else {
                    if (endpoint.bmAttributes == 3) {
                        m_interruptOutSize = endpoint.wMaxPacketSize;
                        m_interruptOutEndpoint = endpoint.bEndpointAddress;
                    }
                }

                ret = libusb_clear_halt(m_handle, endpoint.bEndpointAddress);
                if (ret < 0) {
                    std::cerr << "Failed to clear halt on endpoint: " << libusb_error_name(ret) << std::endl;
                    return false;
                }
            }
        }
    }

    m_inputInterruptXfer = libusb_alloc_transfer(0);
    if (!m_inputInterruptXfer) {
        std::cerr << "Failed to allocate input interrupt transfer" << std::endl;
        return false;
    }
    m_outputInterruptXfer = libusb_alloc_transfer(0);
    if (!m_outputInterruptXfer) {
        std::cerr << "Failed to allocate output interrupt transfer" << std::endl;
        return false;
    }

    m_interruptInBuffer = new uint8_t[m_interruptInSize];
    m_interruptOutBuffer = new uint8_t[m_interruptOutSize];

    libusb_fill_interrupt_transfer(m_inputInterruptXfer, m_handle, m_interruptInEndpoint,
        m_interruptInBuffer, m_interruptInSize, m_inputInterruptCallback, this, 0);

    m_deviceThread = std::thread(&USBDevice::DeviceThread, this);

    m_running = true;

    return true;
}

void USBDevice::Close()
{
    if (m_running) {

        m_running = false;
        libusb_interrupt_event_handler(nullptr);
        if (m_deviceThread.joinable()) {
            m_deviceThread.join();
        }
    }

    if (m_handle) {
        libusb_close(m_handle);
        m_handle = nullptr;
    }

    m_open = false;
}

bool USBDevice::Read(uint8_t *data, size_t size)
{
    if (!m_open) {
        return false;
    }

    int transferred;
    int ret = libusb_bulk_transfer(m_handle, 0x81, data, size, &transferred, 1000);
    if (ret < 0) {
        std::cerr << "Failed to read from USB device: " << libusb_error_name(ret) << std::endl;
        return false;
    }

    return true;
}

bool USBDevice::Write(const uint8_t *data, size_t size)
{
    if (!m_open) {
        return false;
    }

    int transferred;
    int ret = libusb_bulk_transfer(m_handle, 0x01, const_cast<uint8_t*>(data), size, &transferred, 1000);
    if (ret < 0) {
        std::cerr << "Failed to write to USB device: " << libusb_error_name(ret) << std::endl;
        return false;
    }

    return true;
}

void USBDevice::DeviceThread()
{
    while (m_running) {
        int ret = libusb_submit_transfer(m_inputInterruptXfer);
        if (ret < 0) {
            std::cerr << "Failed to submit input interrupt transfer: " << libusb_error_name(ret) << std::endl;
            break;
        }

        ret = libusb_handle_events(nullptr);
        if (ret < 0) {
            std::cerr << "Failed to handle events: " << libusb_error_name(ret) << std::endl;
            break;
        }
    }
}