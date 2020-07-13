#ifndef PM_LOG_H
#define PM_LOG_H

/* System */
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace pm {

class Log
{
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    enum class Level
    {
        Error,
        Warning,
        Info,
        Debug,
        Progress,
    };

    class Entry
    {
        friend class Log;

    public:
        Entry(Level level, const std::string& text);
        virtual ~Entry()
        {}

    public:
        Level GetLevel() const
        { return m_level; }
        std::thread::id GetThreadId() const
        { return m_threadId; }
        const TimePoint& GetTime() const
        { return m_time; }
        const std::string& GetText() const
        { return m_text; }

        std::string GetFormatedMessage() const
        { return m_message; }

    protected:
        virtual void FormatLogMessage();
        virtual std::string GetTimeStamp() const;

    protected:
        Level m_level;
        std::thread::id m_threadId;
        TimePoint m_time;
        std::string m_text;
        std::string m_message;
    };

    class IListener
    {
    public:
        virtual void OnLogEntryAdded(const Entry& entry) = 0;
    };

public:
    // Creates singleton instance
    static Log& Get();
    // Waits until all log entries are processed, but no longer than 1s.
    static bool Flush();
    // Releases singleton instance.
    // Any atempt to get a Log instance after Uninit is called causes
    // application to crash.
    static void Uninit();

private:
    static Log* m_instance;
    static std::once_flag m_onceFlag;

private:
    Log();
    Log(const Log&) = delete;
    Log(Log&&) = delete;
    void operator=(const Log&) = delete;
    void operator=(Log&&) = delete;
    virtual ~Log();

public:
    static void AddListener(IListener* listener);
    static void RemoveListener(IListener* listener);

public:
    static void LogE(const char* format, ...);
    static void LogW(const char* format, ...);
    static void LogI(const char* format, ...);
    static void LogD(const char* format, ...);
    static void LogP(const char* format, ...);

    static void LogE(const std::string& text)
    { return Log::Get().AddEntry(Level::Error, text); }
    static void LogW(const std::string& text)
    { return Log::Get().AddEntry(Level::Warning, text); }
    static void LogI(const std::string& text)
    { return Log::Get().AddEntry(Level::Info, text); }
    static void LogD(const std::string& text)
    { return Log::Get().AddEntry(Level::Debug, text); }
    static void LogP(const std::string& text)
    { return Log::Get().AddEntry(Level::Progress, text); }

    static void AddEntry(Level level, const char* format, ...);
    static void AddEntry(Level level, const char* format, va_list args);
    static void AddEntry(Level level, const std::string& text);
    static void AddEntry(const Entry& entry);

protected:
    void ThreadFunc();

private:
    std::vector<IListener*> m_listeners;
    std::mutex m_listenersMutex;

    std::queue<Entry> m_entries;
    std::mutex m_entriesMutex;
    std::condition_variable m_entriesCond;

    std::thread* m_thread;
    std::atomic<bool> m_threadExitFlag;
};

} // namespace pm

#endif // PM_LOG_H
