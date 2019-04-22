#pragma once
#ifndef PM_FPSLIMITER_H
#define PM_FPSLIMITER_H

/* System */
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

/* Local */
#include "Frame.h"

namespace pm {

class FpsLimiter;

class IFpsLimiterListener
{
public:
    virtual void OnFpsLimiterEvent(FpsLimiter* sender,
            std::shared_ptr<pm::Frame> frame) = 0;
};

class FpsLimiter final
{
public:
    FpsLimiter();
    ~FpsLimiter();

    FpsLimiter(const FpsLimiter&) = delete;
    FpsLimiter& operator=(const FpsLimiter&) = delete;

public:
    bool Start(IFpsLimiterListener* listener);
    bool IsRunning() const;
    void Stop(bool processWaitingFrame = false);

    void InputTimerTick();
    void InputNewFrame(std::shared_ptr<Frame> frame);

private:
    void ThreadLoop();

private:
    IFpsLimiterListener* m_listener;

    std::mutex m_mutex; // Covers all members

    std::thread* m_thread;
    std::atomic<bool> m_abortFlag;

    std::mutex m_eventMutex; // Covers only m_event* members
    std::condition_variable m_eventCond;
    std::atomic<bool> m_eventFlag;

    std::atomic<bool> m_timerEventOn;
    std::atomic<bool> m_frameEventOn;

    std::shared_ptr<Frame> m_frame;
};

} // namespace pm

#endif /* PM_FPSLIMITER_H */
