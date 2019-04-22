#include "Task.h"

/* System */
#include <cassert>

pm::Task::Task(std::shared_ptr<Semaphore> semDone, size_t taskIndex,
        size_t taskCount)
    : m_semaphore(semDone),
    m_taskIndex(taskIndex),
    m_taskCount(taskCount)
{
    assert(m_semaphore != nullptr);
    assert(m_taskCount > 0);
    assert(m_taskIndex < m_taskCount);
}

pm::Task::~Task()
{
}

size_t pm::Task::GetTaskIndex() const
{
    return m_taskIndex;
}

size_t pm::Task::GetTaskCount() const
{
    return m_taskCount;
}

void pm::Task::Done()
{
    m_semaphore->Release();
}
