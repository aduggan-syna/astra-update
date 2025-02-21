#pragma once

#include <fstream>
#include <string>
#include <filesystem>

class Image
{
public:
    Image(std::string imagePath) : m_imagePath{imagePath}
    {
        m_imageName = std::filesystem::path(m_imagePath).filename().string();
    }
    Image(const Image &other) : m_imagePath{other.m_imagePath}, m_imageName{other.m_imageName}
    {}
    ~Image();

    Image &operator=(const Image &other)
    {
        m_imagePath = other.m_imagePath;
        m_imageName = other.m_imageName;
        return *this;
    }

    int Load();

    std::string GetName() const { return m_imageName; }
    std::string GetPath() const { return m_imagePath; }
    int GetDataBlock(uint8_t *data, size_t size);
    int GetSize() const { return m_imageSize; }

private:
    std::string m_imagePath;
    std::string m_imageName;
    size_t m_imageSize;
    std::ifstream m_file;

    FILE *m_fp;
};