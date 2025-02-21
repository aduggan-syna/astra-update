#include "astra_console.hpp"

AstraConsole::~AstraConsole()
{}

void AstraConsole::Append(const std::string &data) {
    std::string trimmedData = data;
    trimmedData.erase(trimmedData.find_last_not_of(" \t\n\r\f\v") + 1);

    if (trimmedData.size() >= 2 && trimmedData.substr(trimmedData.size() - 2) == m_uBootPrompt) {
        std::cout << "U-Boot prompt detected." << std::endl;
        std::unique_lock<std::mutex> lock(m_promptMutex);
        m_promptCV.notify_one();
    }

    m_consoleData += data;
    m_consoleLog << data;
    m_consoleLog.flush();
}

std::string &AstraConsole::Get()
{
    return m_consoleData;
}

bool AstraConsole::WaitForPrompt()
{
    std::unique_lock<std::mutex> lock(m_promptMutex);
    m_promptCV.wait(lock);
    if (m_shutdown.load()) {
        return false;
    }

    return true;
}

void AstraConsole::Shutdown() {
    m_shutdown.store(true);
    m_promptCV.notify_all();
}