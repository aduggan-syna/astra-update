#pragma once

#include <atomic>
#include <string>
#include <iostream>
#include <condition_variable>
#include <mutex>

class AstraConsole
{
public:
    AstraConsole() = default;
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
};