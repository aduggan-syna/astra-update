#pragma once

#include <string>

class Image
{
public:
    Image(std::string imagePath) : m_imagePath{imagePath}
    {}
    ~Image()
    {}

    int Load();

private:
    std::string m_imagePath;
    size_t m_imageSize;
};