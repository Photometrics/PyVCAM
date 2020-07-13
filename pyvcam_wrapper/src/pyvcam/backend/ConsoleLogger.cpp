#include "ConsoleLogger.h"

/* System */
#include <iostream>
#include <sstream>

pm::ConsoleLogger::ConsoleLogger()
{
    Log::AddListener(this);
}

pm::ConsoleLogger::~ConsoleLogger()
{
    Log::RemoveListener(this);
}

void pm::ConsoleLogger::OnLogEntryAdded(const Log::Entry& entry)
{
    static std::mutex mutex;
    static size_t lastProgressMsgLength = 0;
    static bool wasLastMsgProgress = false;

    std::lock_guard<std::mutex> lock(mutex);

    char level = '-';
    switch (entry.GetLevel())
    {
    case Log::Level::Error:
        level = 'E';
        break;
    case Log::Level::Warning:
        level = 'W';
        break;
    case Log::Level::Info:
        level = 'I';
        break;
    case Log::Level::Debug:
        level = 'D';
        break;
    case Log::Level::Progress:
        level = 'P';
        break;
    // default section missing intentionally so compiler warns when new value added
    }

    const std::string msg = std::string("[") + level + "] " + entry.GetText()
        + ((entry.GetLevel() == Log::Level::Progress) ? '\r' : '\n');

    std::ostringstream ss;

    if (wasLastMsgProgress)
    {
        const size_t strFirstNewLinePos = msg.find_first_of("\n");
        std::string strFirstLine = msg.substr(0, strFirstNewLinePos);
        ss << strFirstLine;
        if (strFirstNewLinePos != std::string::npos)
        {
            if (lastProgressMsgLength > strFirstLine.length())
            {
                const std::string clrStr(lastProgressMsgLength - strFirstLine.length(), ' ');
                ss << clrStr;
            }
            ss << msg.substr(strFirstNewLinePos);
        }
    }
    else
    {
        ss << msg;
    }

    if (entry.GetLevel() == Log::Level::Error)
        std::cerr << ss.str() << std::flush;
    else
        std::cout << ss.str() << std::flush;

    lastProgressMsgLength = msg.length();
    wasLastMsgProgress = (entry.GetLevel() == Log::Level::Progress);
}
