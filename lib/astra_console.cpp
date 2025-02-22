#include "astra_console.hpp"
#include "astra_log.hpp"

AstraConsole::~AstraConsole()
{
    ASTRA_LOG;
}

void AstraConsole::Append(const std::string &data)
{
    ASTRA_LOG;

    std::string trimmedData = data;
    trimmedData.erase(trimmedData.find_last_not_of(" \t\n\r\f\v") + 1);

    if (trimmedData.size() >= 2 && trimmedData.substr(trimmedData.size() - 2) == m_uBootPrompt) {
        log(ASTRA_LOG_LEVEL_DEBUG) << "U-Boot prompt detected." << endLog;
        std::unique_lock<std::mutex> lock(m_promptMutex);
        m_promptCV.notify_one();
    }

    m_consoleData += data;
    m_consoleLog << data;
    m_consoleLog.flush();
}

std::string &AstraConsole::Get()
{
    ASTRA_LOG;

    return m_consoleData;
}

bool AstraConsole::WaitForPrompt()
{
    ASTRA_LOG;

    std::unique_lock<std::mutex> lock(m_promptMutex);
    m_promptCV.wait(lock);
    if (m_shutdown.load()) {
        return false;
    }

    return true;
}

void AstraConsole::Shutdown()
{
    ASTRA_LOG;

    m_shutdown.store(true);
    m_promptCV.notify_all();
}