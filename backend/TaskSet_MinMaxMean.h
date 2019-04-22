#pragma once
#ifndef PM_TASK_SET_MIN_MAX_MEAN_H
#define PM_TASK_SET_MIN_MAX_MEAN_H

/* Local */
#include "Frame.h"
#include "Task.h"
#include "TaskSet.h"

/* System */
#include <memory>

namespace pm {

class TaskSet_MinMaxMean : public TaskSet
{
private:
    class ATask : public Task
    {
    public:
        explicit ATask(std::shared_ptr<Semaphore> semDone,
                size_t taskIndex, size_t taskCount);

    public:
        void SetUp(uint16_t* min, uint16_t* max, uint64_t* meanSum,
            uint32_t* pixCount, std::shared_ptr<Frame> frame);

    public: // Task
        virtual void Execute() override;

    private:
        size_t m_maxTasks;
        uint16_t* m_min;
        uint16_t* m_max;
        uint64_t* m_meanSum;
        uint32_t* m_pixCount;
        std::shared_ptr<Frame> m_frame;
    };

public:
    explicit TaskSet_MinMaxMean(std::shared_ptr<ThreadPool> pool);

public:
    void SetUp(uint16_t* min, uint16_t* max, uint16_t* mean,
            std::shared_ptr<Frame> frame);

public: // TaskSet
    virtual void Wait() override;
    template<typename Rep, typename Period>
    bool Wait(const std::chrono::duration<Rep, Period>& timeout);

private:
    void CollectResults();

private:
    uint16_t* m_min;
    uint16_t* m_max;
    uint16_t* m_mean;
    std::unique_ptr<uint16_t[]> m_mins;
    std::unique_ptr<uint16_t[]> m_maxs;
    std::unique_ptr<uint64_t[]> m_meanSums;
    std::unique_ptr<uint32_t[]> m_pixCounts;
};

} // namespace pm

#endif /* PM_TASK_SET_MIN_MAX_MEAN_H */
