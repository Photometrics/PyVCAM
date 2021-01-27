#pragma once
#ifndef PM_FPSLIMITER_H
#define PM_FPSLIMITER_H

/* System */
#include <functional>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

/* Local */
#include "Frame.h"

namespace pm {

// typedef std::function<void(uint32_t)> cb_t;
using FpsLimiterCallbackType = std::function< void(std::shared_ptr<pm::Frame>) >;

// std::function<void(std::shared_ptr<pm::Frame>)>;

class FpsLimiter final
{
public:
    FpsLimiter();
    ~FpsLimiter();

    FpsLimiter(const FpsLimiter&) = delete;
    FpsLimiter& operator=(const FpsLimiter&) = delete;

public:
    bool Start(FpsLimiterCallbackType callback_function);
    bool IsRunning() const;
    void Stop(bool processWaitingFrame = false);

    void InputTimerTick();
    void InputNewFrame(std::shared_ptr<Frame> frame);

private:
    void ThreadLoop();

private:
    FpsLimiterCallbackType m_callback_func = nullptr;

    std::mutex m_mutex; // Covers all members

    std::thread* m_thread = nullptr;
    std::atomic<bool> m_abortFlag = false;

    std::mutex m_eventMutex; // Covers only m_event* members
    std::condition_variable m_eventCond;
    std::atomic<bool> m_eventFlag = false;

    std::atomic<bool> m_timerEventOn = false;
    std::atomic<bool> m_frameEventOn = false;

    std::shared_ptr<Frame> m_frame;
};

} // namespace pm

#endif /* PM_FPSLIMITER_H */
