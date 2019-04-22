#include "ThreadPool.h"

/* Local */
#include "Task.h"

/* System */
#include <cassert>

pm::ThreadPool::ThreadPool(size_t size)
    : m_threads(),
    m_abortFlag(false),
    m_queue(),
    m_queueMutex(),
    m_queueCond()
{
    assert(size > 0);

    m_threads.reserve(size);
    for (size_t n = 0; n < size; ++n)
    {
        auto thread = std::make_unique<std::thread>(&ThreadPool::ThreadFunc, this);
        m_threads.push_back(std::move(thread));
    }
}

pm::ThreadPool::~ThreadPool()
{
    RequestAbort();
    WaitAborted();
}

size_t pm::ThreadPool::GetSize() const
{
    return m_threads.size();
}

void pm::ThreadPool::Execute(Task* task)
{
    assert(task != nullptr);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_abortFlag)
            return;
        m_queue.push(task);
    }
    m_queueCond.notify_one();
}

void pm::ThreadPool::Execute(const std::vector<Task*>& tasks)
{
    assert(!tasks.empty());

    if (tasks.size() == 1)
    {
        Execute(tasks[0]);
    }
    else
    {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_abortFlag)
                return;
            for (auto task : tasks)
            {
                m_queue.push(task);
            }
        }
        m_queueCond.notify_all();
    }
}

void pm::ThreadPool::RequestAbort()
{
    m_abortFlag = true;
    m_queueCond.notify_all();
}

void pm::ThreadPool::WaitAborted()
{
    for (auto& thread : m_threads)
    {
        thread->join();
    }
    m_threads.clear();
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // Some tasks were left infinished, don't call task->Done(),
        // just clear them out
        std::queue<Task*>().swap(m_queue);
    }
}

void pm::ThreadPool::ThreadFunc()
{
    while (!m_abortFlag)
    {
        Task* task = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            if (m_queue.empty())
            {
                m_queueCond.wait(lock, [this] {
                    return (!m_queue.empty() || m_abortFlag);
                });
            }
            if (m_abortFlag)
                break;

            task = m_queue.front();
            m_queue.pop();
        }

        task->Execute();
        task->Done();
    }
}
