#include "TaskSet.h"

/* Local */
#include "Task.h"

/* System */
#include <cassert>

pm::TaskSet::TaskSet(std::shared_ptr<ThreadPool> pool)
    : m_pool(pool),
    m_tasks(),
    m_semaphore(std::make_shared<Semaphore>())
{
    assert(m_pool != nullptr);
    assert(m_semaphore != nullptr);
}

pm::TaskSet::~TaskSet()
{
    ClearTasks();
}

std::shared_ptr<pm::ThreadPool> pm::TaskSet::GetThreadPool() const
{
    return m_pool;
}

const std::vector<pm::Task*>& pm::TaskSet::GetTasks() const
{
    return m_tasks;
}

void pm::TaskSet::Execute()
{
    m_pool->Execute(m_tasks);
}

void pm::TaskSet::Wait()
{
    m_semaphore->Wait(m_tasks.size());
}

template<typename Rep, typename Period>
bool pm::TaskSet::Wait(const std::chrono::duration<Rep, Period>& timeout)
{
    return m_semaphore->Wait(timeout, m_tasks.size());
}

void pm::TaskSet::ClearTasks()
{
    for (auto task : m_tasks)
    {
        delete task;
    }
    m_tasks.clear();
}
