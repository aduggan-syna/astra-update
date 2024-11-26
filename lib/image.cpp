#include <filesystem>
#include <vector>
#include <iostream>
#include <cstring>

#include "image.hpp"

int Image::Load()
{
    std::cout << "Loading image: " << m_imagePath << std::endl;
    m_imageName = std::filesystem::path(m_imagePath).filename().string();

    FILE *fp = fopen(m_imagePath.c_str(), "rb");
    if (fp == nullptr) {
        std::cerr << "Failed to open file: " << m_imagePath << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        return -1;
    }

    uint32_t size;
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    std::cout << "Image size: " << size << std::endl;

    m_imageSize = size;

    m_fp = fp;

    return 0;
}

int Image::GetDataBlock(uint8_t *data, size_t size)
{
    int readSize = size;
    if (m_imageSize < size) {
        readSize = m_imageSize;
    }

    long currentPos = ftell(m_fp);
    if (currentPos + static_cast<long>(readSize) > m_imageSize) {
        readSize = m_imageSize - currentPos;
    }

    size_t bytesRead = fread(data, 1, readSize, m_fp);
    if (bytesRead != readSize) {
        return -1;
    }

    return readSize;
}

Image::~Image()
{
    m_file.close();
}