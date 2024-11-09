#pragma once

#include <string>
#include <iostream>
#include <condition_variable>
#include <mutex>

class Console
{
public:
    Console() = default;
    ~Console();

    void Append(const std::string &data);
    std::string &Get();

    void WaitForPrompt();

    void SendCommand(const std::string &command);

private:
    std::string m_consoleData;
    const std::string m_uBootPrompt = "=>";
    std::condition_variable m_promptCV;
    std::mutex m_promptMutex;
};