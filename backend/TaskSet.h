#pragma once
#ifndef PM_TASK_SET_H
#define PM_TASK_SET_H

/* Local */
#include "Semaphore.h"
#include "ThreadPool.h"

/* System */
#include <chrono>
#include <memory> // std::shared_ptr
#include <vector>

namespace pm {

class Task;

class TaskSet
{
public:
    // Constructor calls CreateTasks method
    explicit TaskSet(std::shared_ptr<ThreadPool> pool);
    virtual ~TaskSet();

public:
    std::shared_ptr<ThreadPool> GetThreadPool() const;
    const std::vector<Task*>& GetTasks() const;

    // Every child task-set implements some SetUp method
    //void SetUp(<params>);

    virtual void Execute();

    virtual void Wait();
    // Member function templates cannot be virtual
    template<typename Rep, typename Period>
    bool Wait(const std::chrono::duration<Rep, Period>& timeout);

protected:
    // Can be called by each TaskSet subclass constructor as default impl.
    // Otherwise the virtual CreateTasks method has to be overridden.
    template<class T,
        // Private param to enforce the type T derives from Task class
        typename std::enable_if<std::is_base_of<Task, T>::value, int>::type = 0>
    void CreateTasks()
    {
        if (CreateTasks(m_semaphore, m_tasks))
            return; // Subclass has custom implementation

        ClearTasks();

        const size_t taskCount = m_pool->GetSize();
        m_tasks.reserve(taskCount);
        for (size_t n = 0; n < taskCount; ++n)
        {
            auto task = new (std::nothrow) T(m_semaphore, n, taskCount);
            if (!task)
                continue;
            m_tasks.push_back(task);
        }
    }

    // Called by default CreateTasks method.
    // Method should return true if overridden.
    // semDone should be only passed to Task constructor.
    virtual bool CreateTasks(std::shared_ptr<Semaphore> /*semDone*/,
            std::vector<Task*>& /*tasks*/)
    { return false; }

    virtual void ClearTasks();

private:
    std::shared_ptr<ThreadPool> m_pool;
    std::vector<Task*> m_tasks;
    std::shared_ptr<Semaphore> m_semaphore;
};

} // namespace pm

#endif /* PM_TASK_SET_H */
