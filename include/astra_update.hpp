#pragma once

#include <string>

class BootFirmwareCollection;
class USBTransport;

class AstraUpdate
{
public:
    AstraUpdate(std::string bootFirmwarePath, std::string osImage);
    ~AstraUpdate();

    int Run();

private:
    std::string m_bootFirmwarePath;
    std::string m_osImage;
    BootFirmwareCollection *m_bootFirmwares;
    USBTransport *m_transport;
};