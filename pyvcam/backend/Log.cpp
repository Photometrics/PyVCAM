#ifdef _MSC_VER
// Suppress warning C4996 on vsnprintf functions
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Log.h"

/* System */
#include <algorithm>
#include <cstdarg>
#include <cstring> // std::strncpy
#include <ctime>
#include <iomanip>
#include <sstream>

pm::Log* pm::Log::m_instance = nullptr;
std::once_flag pm::Log::m_onceFlag;

pm::Log& pm::Log::Get()
{
    std::call_once(m_onceFlag, []() {
        m_instance = new(std::nothrow) Log();
    });
    return *m_instance;

    // TODO: Current solution causes memory leak of m_instance at app. exit.
    //       When changed to smart pointer or local static variable (as shown
    //       below) it causes a hang in m_thread->join even though the thread
    //       function exits gracefully. The worst thing is that this happens
    //       only in case the app. is closed right after opening.
    //       It seems that wxWidgets code initiates memory leak dumps because
    //       other apps using the same pattern do not report any leak.
    //       Retest later with MSVS 2015.

    // This is thread-safe in C++11, but on Windows only since MSVS 2015
    //static Log instance;
    //return instance;
}

bool pm::Log::Flush()
{
    std::unique_lock<std::mutex> lock(Get().m_entriesMutex);

    // Wait until all log entries are processed, but no longer than 1s
    int count = 10;
    while (!Get().m_entries.empty() && count-- > 0)
    {
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        lock.lock();
    }

    return Get().m_entries.empty();
}

void pm::Log::Uninit()
{
    // Release the singleton instance, from now on all calls to Get causes crash
    delete m_instance;
    m_instance = nullptr;
}

pm::Log::Log()
    : m_listeners(),
    m_listenersMutex(),
    m_entries(),
    m_entriesMutex(),
    m_entriesCond(),
    m_thread(nullptr),
    m_threadExitFlag(false)
{
    m_thread = new(std::nothrow) std::thread(&pm::Log::ThreadFunc, this);
}

pm::Log::~Log()
{
    m_threadExitFlag = true;
    m_entriesCond.notify_one(); // Unblock
    if (m_thread->joinable())
        m_thread->join();
    delete m_thread;
}

void pm::Log::AddListener(IListener* listener)
{
    std::unique_lock<std::mutex> lock(Get().m_listenersMutex);

    if (!listener)
        return;

    auto it =
        std::find(Get().m_listeners.begin(), Get().m_listeners.end(), listener);

    if (it != Get().m_listeners.end())
        return;  // Already registered

    Get().m_listeners.push_back(listener);
}

void pm::Log::RemoveListener(IListener* listener)
{
    std::unique_lock<std::mutex> lock(Get().m_listenersMutex);

    auto it =
        std::find(Get().m_listeners.begin(), Get().m_listeners.end(), listener);

    if (it == Get().m_listeners.end())
        return;  // Not registered

    Get().m_listeners.erase(it);
}

void pm::Log::LogE(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    AddEntry(Level::Error, format, args);
    va_end(args);
}

void pm::Log::LogW(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    AddEntry(Level::Warning, format, args);
    va_end(args);
}

void pm::Log::LogI(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    AddEntry(Level::Info, format, args);
    va_end(args);
}

void pm::Log::LogD(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    AddEntry(Level::Debug, format, args);
    va_end(args);
}

void pm::Log::LogP(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    AddEntry(Level::Progress, format, args);
    va_end(args);
}

void pm::Log::AddEntry(Level level, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    AddEntry(level, format, args);
    va_end(args);
}

void pm::Log::AddEntry(Level level, const char* format, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    std::vector<char> buffer(1 + std::vsnprintf(NULL, 0, format, args2));
    va_end(args2);
    std::vsnprintf(buffer.data(), buffer.size(), format, args);
    AddEntry(level, std::string(buffer.data()));
}

void pm::Log::AddEntry(Level level, const std::string& text)
{
    Entry entry(level, text);
    AddEntry(entry);
}

void pm::Log::AddEntry(const Entry& entry)
{
    {
        std::unique_lock<std::mutex> lock(Get().m_entriesMutex);
        Get().m_entries.push(entry);
    }
    Get().m_entriesCond.notify_one();
}

void pm::Log::ThreadFunc()
{
    Entry entry(Level::Error, "UNINITIALIZED LOG ENTRY");
    while (!m_threadExitFlag)
    {
        {
            std::unique_lock<std::mutex> lock(m_entriesMutex);

            if (m_entries.empty())
            {
                m_entriesCond.wait(lock, [this]() {
                    return (m_threadExitFlag || !m_entries.empty());
                });
            }
            if (m_threadExitFlag)
                break;

            entry = m_entries.front();
            m_entries.pop();
        }

        entry.FormatLogMessage();

        // Notify listeners
        {
            std::unique_lock<std::mutex> lock(m_listenersMutex);

            for (auto listener : m_listeners)
            {
                listener->OnLogEntryAdded(entry);
            }
        }
    }
}

pm::Log::Entry::Entry(Level level, const std::string& text)
    : m_level(level),
    m_threadId(std::this_thread::get_id()),
    m_time(Clock::now()),
    m_text(text),
    m_message()
{
}

void pm::Log::Entry::FormatLogMessage()
{
    // Log format:
    // [20151231-232359.999][89ABCDEF][E] TEXT
    //            |             |      |
    //            |             |      \- Level
    //            |             \-------- Thread ID
    //            \---------------------- Time stamp

    char level = '?';
    switch (m_level)
    {
    case Level::Error:
        level = 'E';
        break;
    case Level::Warning:
        level = 'W';
        break;
    case Level::Info:
        level = 'I';
        break;
    case Level::Debug:
        level = 'D';
        break;
    case Level::Progress:
        level = 'P';
        break;
    // default section missing intentionally so compiler warns when new value added
    }

    std::ostringstream ss;
    ss << std::setfill('0')
        // Time stamp
        << GetTimeStamp()
        // Thread ID
        << '[' << std::setw(8) << std::hex << m_threadId << ']'
        // Level
        << '[' << level << ']'
        // Text
        << ' ' << m_text;

    m_message = ss.str();
}

std::string pm::Log::Entry::GetTimeStamp() const
{
    const std::time_t time = Clock::to_time_t(m_time);

    // Thread-safe conversion to local time
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else // POSIX
    localtime_r(&time, &tm);
#endif

    const unsigned int msec =
        std::chrono::time_point_cast<std::chrono::milliseconds>(m_time)
        .time_since_epoch().count() % 1000;

    // std::put_time is not available in GCC < 5.0, using strftime instead.
    static const char format[] = "YYYYMMDD-HHMMSS";
    static const char formatStr[] = "%Y%m%d-%H%M%S";
    char buffer[sizeof(format)];
    if (sizeof(format) - 1 != std::strftime(buffer, sizeof(buffer), formatStr, &tm))
    {
        std::strncpy(buffer, "YYYYMMDD-HHMMSS", sizeof(buffer));
    }
    std::ostringstream ssTimeStamp;
    ssTimeStamp << std::setfill('0')
        << '['
        //<< std::put_time(&tm, formatStr)
        << buffer
        << '.' << std::setw(3) << msec
        << ']';

    return ssTimeStamp.str();
}
