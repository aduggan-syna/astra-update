#pragma once

#include <atomic>
#include <string>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <fstream>

class AstraConsole
{
public:
    AstraConsole(std::string deviceName, std::string logPath)
    {
        std::string logFile = logPath + "/console-" + deviceName + ".log";
        m_consoleLog = std::ofstream(logFile);
    }

    ~AstraConsole();

    void Append(const std::string &data);
    std::string &Get();

    bool WaitForPrompt();
    void Shutdown();

private:
    std::string m_consoleData;
    const std::string m_uBootPrompt = "=>";
    std::condition_variable m_promptCV;
    std::mutex m_promptMutex;
    std::atomic<bool> m_shutdown{false};
    std::ofstream m_consoleLog;
};