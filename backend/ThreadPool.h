#pragma once
#ifndef PM_THREAD_POOL_H
#define PM_THREAD_POOL_H

/* System */
#include <atomic>
#include <condition_variable>
#include <memory> // std::unique_ptr
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace pm {

class Task;

class ThreadPool final
{
public:
    explicit ThreadPool(size_t size);
    ~ThreadPool();

public:
    size_t GetSize() const;

    void Execute(Task* task);
    void Execute(const std::vector<Task*>& tasks);

    void RequestAbort();
    void WaitAborted();

protected:
    void ThreadFunc();

private:
    std::vector<std::unique_ptr<std::thread>> m_threads;
    std::atomic<bool>                         m_abortFlag;

    std::queue<Task*>       m_queue;
    std::mutex              m_queueMutex;
    std::condition_variable m_queueCond;
};

} // namespace pm

#endif /* PM_THREAD_POOL_H */
