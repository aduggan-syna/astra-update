#pragma once

#include <string>
#include <memory>
#include <functional>

class AstraUpdate {
public:
    AstraUpdate(std::string flashImage, std::string bootFirmwarePath);
    ~AstraUpdate();

    void RegisterStatusCallback(std::function<void(int status, int progress, const std::string& message)> callback);

    int Update();

private:
    class AstraUpdateImpl;
    std::unique_ptr<AstraUpdateImpl> pImpl;
};