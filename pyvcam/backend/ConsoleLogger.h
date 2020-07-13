#pragma once
#ifndef PM_CONSOLE_LOGGER_H
#define PM_CONSOLE_LOGGER_H

/* Local */
#include "Log.h"

namespace pm {

class ConsoleLogger : private Log::IListener
{
public:
    ConsoleLogger();
    virtual ~ConsoleLogger();

    ConsoleLogger(const ConsoleLogger&) = delete;
    ConsoleLogger& operator=(const ConsoleLogger&) = delete;

private: // Log::IListener
    virtual void OnLogEntryAdded(const Log::Entry& entry) override;
};

} // namespace pm

#endif /* PM_CONSOLE_LOGGER_H */
