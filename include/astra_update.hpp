#pragma once

#include <string>
#include <memory>

class AstraUpdate {
public:
    AstraUpdate(std::string bootFirmwarePath, std::string osImage);
    ~AstraUpdate();

    int Run();

private:
    class AstraUpdateImpl;
    std::unique_ptr<AstraUpdateImpl> pImpl;
};