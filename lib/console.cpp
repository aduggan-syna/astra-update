#include "console.hpp"

Console::~Console()
{}

void Console::Append(const std::string &data) {
    std::string trimmedData = data;
    trimmedData.erase(trimmedData.find_last_not_of(" \t\n\r\f\v") + 1);

    if (trimmedData.size() >= 2 && trimmedData.substr(trimmedData.size() - 2) == m_uBootPrompt) {
        std::cout << "U-Boot prompt detected." << std::endl;
        std::unique_lock<std::mutex> lock(m_promptMutex);
        m_promptCV.notify_one();
    }

    m_consoleData += data;
    std::cout << "Console: '" << m_consoleData << "'" << std::endl;
}

std::string &Console::Get()
    {
    return m_consoleData;
}

void Console::WaitForPrompt()
{
    std::unique_lock<std::mutex> lock(m_promptMutex);
    m_promptCV.wait(lock);
}

void Console::SendCommand(const std::string &command)
{

}