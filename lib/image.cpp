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
#if 0
    m_file.open(m_imagePath, std::ios::binary);
    if (!m_file.is_open() && m_file.fail()) {
        return -1; // Error opening file
    }

    if (m_file.fail()) {
        m_file.clear();
    #if 0
        std::cerr << "Failed to open file: " << m_imagePath << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        return -1; // Error opening file
    #endif
    }

    std::streamsize size = m_file.seekg(0, std::ios::end).tellg();
    if (size < 0) {
        return -1; // Error getting file size
    }
    m_imageSize = size;

    m_file.seekg(0, std::ios::beg);
#endif

    return 0;
}

int Image::GetDataBlock(uint8_t *data, size_t size)
{
    int readSize = size;
    if (m_imageSize < size) {
        readSize = m_imageSize;
    }

    if (ftell(m_fp) + static_cast<long>(readSize) > m_imageSize) {
        return -1;
    }

    size_t bytesRead = fread(data, 1, readSize, m_fp);
    if (bytesRead != readSize) {
        return -1;
    }

#if 0
    std::streampos offset = m_file.tellg();
    if (offset + static_cast<std::streampos>(readSize) > m_imageSize) {
        return -1;
    }

    m_file.read(reinterpret_cast<char*>(data), readSize);
    if (!m_file) {
        return -1;
    }
#endif

    return readSize;
}

Image::~Image()
{
    m_file.close();
}