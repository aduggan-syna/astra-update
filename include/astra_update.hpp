#pragma once

#include <string>

class BootImageCollection;
class USBTransport;

class AstraUpdate
{
public:
    AstraUpdate(std::string bootImagePath, std::string firmwareImage);
    ~AstraUpdate();

    int Run();

private:
    std::string m_bootImagePath;
    std::string m_firmwareImage;
    BootImageCollection *m_bootImages;
    USBTransport *m_transport;
};