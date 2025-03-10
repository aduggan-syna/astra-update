#include "win_usb_transport.hpp"
#include "astra_log.hpp"
#include <initguid.h>
#include <devpkey.h>
#include <usbiodef.h>

WinUSBTransport::~WinUSBTransport()
{
    ASTRA_LOG;
    Shutdown();
}

int WinUSBTransport::Init(uint16_t vendorId, uint16_t productId, std::function<void(std::unique_ptr<USBDevice>)> deviceAddedCallback)
{
    ASTRA_LOG;

    m_vendorId = vendorId;
    m_productId = productId;

    int ret = libusb_init(&m_ctx);
    if (ret < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to initialize libusb: " << libusb_error_name(ret) << endLog;
        return ret;
    }

    if (m_usbDebug) {
        libusb_set_option(m_ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
    }

    m_deviceAddedCallback = deviceAddedCallback;

    m_running.store(true);

    m_hotplugThread = std::thread(&WinUSBTransport::RunHotplugHandler, this);

    StartDeviceMonitor();

    return ret;
}

void WinUSBTransport::Shutdown()
{
    ASTRA_LOG;

    std::lock_guard<std::mutex> lock(m_shutdownMutex);
    if (m_running.exchange(false)) {
        if (m_hWnd) {
            PostMessage(m_hWnd, WM_QUIT, 0, 0);
            if (m_hotplugThread.joinable()) {
                m_hotplugThread.join();
            }
            DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
        }

        if (m_deviceMonitorThread.joinable()) {
            m_deviceMonitorThread.join();
        }

        if (m_ctx) {
            libusb_exit(m_ctx);
        }
    }
}

void WinUSBTransport::RunHotplugHandler()
{
    ASTRA_LOG;

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WinUSBTransport::WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = TEXT("WinUSBTransport");

    if (!RegisterClass(&wc)) {
        DWORD error = GetLastError();
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to register window class: " << error << endLog;
        return;
    }

    m_hWnd = CreateWindow(wc.lpszClassName, TEXT("WinUSBTransport"), 0, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, this);
    if (!m_hWnd) {
        DWORD error = GetLastError();
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to create window: " << error << endLog;
        return;
    }

    DEV_BROADCAST_DEVICEINTERFACE dbi = { 0 };
    dbi.dbcc_size = sizeof(dbi);
    dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbi.dbcc_reserved = 0;
    dbi.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    m_hDevNotify = RegisterDeviceNotification(m_hWnd, &dbi, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!m_hDevNotify) {
        DWORD error = GetLastError();
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to register device notification: " << error << endLog;
        return;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK WinUSBTransport::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DEVICECHANGE) {
        PDEV_BROADCAST_HDR pHdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);
        if (wParam == DBT_DEVICEARRIVAL && pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
            WinUSBTransport* handler = reinterpret_cast<WinUSBTransport*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            if (handler) {
                handler->OnDeviceArrived();
            }
        }
    } else if (message == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void WinUSBTransport::OnDeviceArrived()
{
    ASTRA_LOG;

    // Handle device arrival
    // You can use libusb to get the device list and find the new device
    libusb_device **device_list;
    ssize_t count = libusb_get_device_list(m_ctx, &device_list);
    if (count < 0) {
        log(ASTRA_LOG_LEVEL_ERROR) << "Failed to get device list: " << libusb_error_name(count) << endLog;
        return;
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
}