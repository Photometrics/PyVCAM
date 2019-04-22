#pragma once
#ifndef PM_TASK_H
#define PM_TASK_H

/* Local */
#include "Semaphore.h"

/* System */
#include <memory> // std::shared_ptr

namespace pm {

class Task
{
public:
    explicit Task(std::shared_ptr<Semaphore> semDone, size_t taskIndex,
            size_t taskCount);
    virtual ~Task();

public:
    size_t GetTaskIndex() const;
    size_t GetTaskCount() const;

    // Every child task implements some SetUp method
    //void SetUp(<parameters>);

public:
    virtual void Execute() = 0;

    // Called by ThreadPool after Execute method finishes
    virtual void Done();

private:
    std::shared_ptr<Semaphore> m_semaphore;
    size_t m_taskIndex;
    size_t m_taskCount;
};

} // namespace pm

#endif /* PM_TASK_H */
