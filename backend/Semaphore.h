#pragma once
#ifndef PM_SEMAPHORE_H
#define PM_SEMAPHORE_H

/* System */
#include <condition_variable>
#include <mutex>

namespace pm {

class Semaphore final
{
public:
    explicit Semaphore();
    explicit Semaphore(size_t initCount);
    ~Semaphore();

public:
    void Wait(size_t count = 1);

    template<typename Rep, typename Period>
    bool Wait(const std::chrono::duration<Rep, Period>& timeout,
            size_t count = 1);

    void Release(size_t count = 1);

private:
    size_t m_count;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};

} // namespace pm

#endif /* PM_SEMAPHORE_H */
