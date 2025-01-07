#pragma once

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

    void WaitForPrompt();

private:
    std::string m_consoleData;
    const std::string m_uBootPrompt = "=>";
    std::condition_variable m_promptCV;
    std::mutex m_promptMutex;
};