#include "Semaphore.h"

pm::Semaphore::Semaphore()
    : m_count(0),
    m_mutex(),
    m_cond()
{
}

pm::Semaphore::Semaphore(size_t initCount)
    : m_count(initCount),
    m_mutex(),
    m_cond()
{
}

pm::Semaphore::~Semaphore()
{
    Release(m_count);
    // TODO: Wait for proper release
}

void pm::Semaphore::Wait(size_t count)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock, [&]() { return m_count >= count; });
    m_count -= count;
}

template<typename Rep, typename Period>
bool pm::Semaphore::Wait(const std::chrono::duration<Rep, Period>& timeout,
        size_t count)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const bool timedOut =
        !m_cond.wait_for(lock, timeout, [&]() { return m_count >= count; });
    if (timedOut)
        return false;
    m_count -= count;
    return true;
}

void pm::Semaphore::Release(size_t count)
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_count += count;
    }
    m_cond.notify_one();
}
