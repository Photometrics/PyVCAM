#include "FpsLimiter.h"

/* System */
#include <algorithm>

pm::FpsLimiter::FpsLimiter()
{
}

pm::FpsLimiter::~FpsLimiter()
{
    Stop();
}

bool pm::FpsLimiter::Start(FpsLimiterCallbackType callback_func)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_thread)
        return true;

    if (!callback_func)
        return false;

    m_callback_func = callback_func;

    m_frame = nullptr;

    // Set the flag so when first frame arrives, we will display it immediately
    m_timerEventOn = true;
    m_frameEventOn = false;

    m_abortFlag = false;
    m_thread = new(std::nothrow) std::thread(&FpsLimiter::ThreadLoop, this);

    return (m_thread != nullptr);
}

bool pm::FpsLimiter::IsRunning() const
{
    return (m_thread != nullptr);
}

void pm::FpsLimiter::Stop(bool processLastWaitingFrame)
{
    if (!m_thread)
        return;

    bool frameEventOn;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        frameEventOn = m_frameEventOn;
    }

    {
        std::unique_lock<std::mutex> lock(m_eventMutex);
        m_eventFlag = processLastWaitingFrame && frameEventOn;
        m_abortFlag = true;
    }
    m_eventCond.notify_one();

    if (m_thread->joinable())
        m_thread->join();

    {
        std::unique_lock<std::mutex> lock(m_mutex);

        delete m_thread;
        m_thread = nullptr;

        m_frame = nullptr;
    }

    m_callback_func = nullptr;
}

void pm::FpsLimiter::InputTimerTick()
{
    bool frameEventOn;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        frameEventOn = m_frameEventOn;
        m_timerEventOn = true;
    }

    if (frameEventOn)
    {
        {
            std::unique_lock<std::mutex> lock(m_eventMutex);
            if (m_eventFlag)
                return;
            m_eventFlag = true;
        }
        m_eventCond.notify_one();
    }
}

void pm::FpsLimiter::InputNewFrame(std::shared_ptr<Frame> frame)
{
    bool timerEventOn;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        timerEventOn = m_timerEventOn;
        m_frameEventOn = true;

        m_frame = frame;
    }

    if (timerEventOn)
    {
        {
            std::unique_lock<std::mutex> lock(m_eventMutex);
            if (m_eventFlag)
                return;
            m_eventFlag = true;
        }
        m_eventCond.notify_one();
    }
}

void pm::FpsLimiter::ThreadLoop()
{
    std::shared_ptr<Frame> frame;

    while (!m_abortFlag)
    {
        {
            std::unique_lock<std::mutex> lock(m_eventMutex);
            if (!m_eventFlag)
            {
                m_eventCond.wait(lock, [this]() {
                    return (m_abortFlag || m_eventFlag);
                });
            }
            if (m_abortFlag && !m_eventFlag)
                break;
            m_eventFlag = false;
        }

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_timerEventOn = false;
            m_frameEventOn = false;
            // Need a copy as m_frame can be changed while entering OnFpsLimiterEvent
            frame = m_frame;
        }

        m_callback_func(frame);
        frame = nullptr;
    }
}
