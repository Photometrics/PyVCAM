#include "TaskSet_MinMaxMean.h"

/* System */
#include <cassert>
#include <numeric>

// TaskSet_MinMaxMean::Task

pm::TaskSet_MinMaxMean::ATask::ATask(
        std::shared_ptr<Semaphore> semDone, size_t taskIndex, size_t taskCount)
    : pm::Task(semDone, taskIndex, taskCount),
    m_maxTasks(taskCount),
    m_min(nullptr),
    m_max(nullptr),
    m_meanSum(nullptr),
    m_pixCount(nullptr),
    m_frame()
{
}

void pm::TaskSet_MinMaxMean::ATask::SetUp(uint16_t* min, uint16_t* max,
        uint64_t* meanSum, uint32_t* pixCount, std::shared_ptr<Frame> frame)
{
    assert(min != nullptr);
    assert(max != nullptr);
    assert(meanSum != nullptr);
    assert(pixCount != nullptr);
    assert(frame != nullptr);

    uint32_t pixelCount = (std::numeric_limits<uint32_t>::max)();

    const Frame::AcqCfg& acqCfg = frame->GetAcqCfg();
    const md_frame* frameMeta = frame->GetMetadata();

    if (!acqCfg.HasMetadata())
    {
        pixelCount = (uint32_t)(acqCfg.GetFrameBytes() / sizeof(uint16_t));
    }
    else if (frameMeta->roiCount > 0)
    {
        // For simplicity using only first ROI and ignoring its flags
        const rgn_type& rgn = frameMeta->roiArray[0].header->roi;
        if (rgn.sbin == 0 || rgn.pbin == 0)
        {
            pixelCount = 0;
        }
        else
        {
            const uint16_t w = (rgn.s2 - rgn.s1 + 1) / rgn.sbin;
            const uint16_t h = (rgn.p2 - rgn.p1 + 1) / rgn.pbin;
            pixelCount = w * h;
        }
    }
    else
    {
        pixelCount = 0;
    }

    m_maxTasks = (pixelCount < 100)
        ? (pixelCount == 0) ? 0 : 1
        : GetTaskCount();

    m_min = min;
    m_max = max;
    m_meanSum = meanSum;
    m_pixCount = pixCount;
    m_frame = frame;
}

void pm::TaskSet_MinMaxMean::ATask::Execute()
{
    *m_min = 0;
    *m_max = 0;
    *m_meanSum = 0;
    *m_pixCount = 0;

    if (GetTaskIndex() >= m_maxTasks)
        return;

    const uint8_t step = (uint8_t)m_maxTasks;
    const uint8_t offset = (uint8_t)GetTaskIndex();

    // Prepare reversed illogical values as starting point
    uint16_t min = (std::numeric_limits<uint16_t>::max)();
    uint16_t max = (std::numeric_limits<uint16_t>::min)();
    uint64_t meanSum = 0;
    uint32_t pixCount = 0;

    const Frame::AcqCfg& acqCfg = m_frame->GetAcqCfg();
    const md_frame* frameMeta = m_frame->GetMetadata();

    if (!acqCfg.HasMetadata())
    {
        if (acqCfg.GetFrameBytes() > 0)
        {
            const uint32_t count =
                (uint32_t)(acqCfg.GetFrameBytes() / sizeof(uint16_t));
            uint16_t* data = (uint16_t*)m_frame->GetData() + offset;
            const uint16_t* dataEnd = (uint16_t*)m_frame->GetData() + count;

            while (data < dataEnd)
            {
                if (min > *data)
                    min = *data;
                else if (max < *data)
                    max = *data;
                meanSum += *data;
                pixCount += 1;
                data += step;
            }
        }
    }
    else
    {
        for (int n = 0; n < frameMeta->roiCount; ++n)
        {
            const md_frame_roi& roi = frameMeta->roiArray[n];
            if (roi.header->flags & PL_MD_ROI_FLAG_HEADER_ONLY)
                continue;
            const rgn_type& rgn = roi.header->roi;
            const uint16_t w = (rgn.s2 - rgn.s1 + 1) / rgn.sbin;
            const uint16_t h = (rgn.p2 - rgn.p1 + 1) / rgn.pbin;

            const uint32_t count = w * h;
            uint16_t* data = (uint16_t*)roi.data + offset;
            const uint16_t* dataEnd = (uint16_t*)roi.data + count;

            while (data < dataEnd)
            {
                if (min > *data)
                    min = *data;
                else if (max < *data)
                    max = *data;
                meanSum += *data;
                pixCount += 1;
                data += step;
            }
        }
    }

    if (pixCount > 0)
    {
        *m_min = min;
        *m_max = max;
        *m_meanSum = meanSum;
        *m_pixCount = pixCount;
    }
}

// TaskSet_MinMaxMean

pm::TaskSet_MinMaxMean::TaskSet_MinMaxMean(
        std::shared_ptr<ThreadPool> pool)
    : TaskSet(pool),
    m_min(nullptr),
    m_max(nullptr),
    m_mean(nullptr),
    m_mins(nullptr),
    m_maxs(nullptr),
    m_meanSums(nullptr),
    m_pixCounts(nullptr)
{
    CreateTasks<ATask>();

    const size_t taskCount = GetTasks().size();
    m_mins = std::make_unique<uint16_t[]>(taskCount);
    m_maxs = std::make_unique<uint16_t[]>(taskCount);
    m_meanSums = std::make_unique<uint64_t[]>(taskCount);
    m_pixCounts = std::make_unique<uint32_t[]>(taskCount);
}

void pm::TaskSet_MinMaxMean::SetUp(uint16_t* min, uint16_t* max, uint16_t* mean,
        std::shared_ptr<Frame> frame)
{
    assert(min != nullptr);
    assert(max != nullptr);
    assert(mean != nullptr);

    m_min = min;
    m_max = max;
    m_mean = mean;

    const auto tasks = GetTasks();
    const size_t taskCount = tasks.size();
    for (size_t n = 0; n < taskCount; ++n)
    {
        static_cast<ATask*>(tasks[n])->SetUp(
                &m_mins[n], &m_maxs[n], &m_meanSums[n], &m_pixCounts[n], frame);
    }
}

void pm::TaskSet_MinMaxMean::Wait()
{
    TaskSet::Wait();
    CollectResults();
}

template<typename Rep, typename Period>
bool pm::TaskSet_MinMaxMean::Wait(const std::chrono::duration<Rep, Period>& timeout)
{
    const bool retVal = TaskSet::Wait(timeout);
    CollectResults();
    return retVal;
}

void pm::TaskSet_MinMaxMean::CollectResults()
{
    // Prepare reversed illogical values as starting point
    uint16_t min = (std::numeric_limits<uint16_t>::max)();
    uint16_t max = (std::numeric_limits<uint16_t>::min)();
    uint64_t meanSum = 0;
    uint32_t pixCount = 0;

    const size_t taskCount = GetTasks().size();
    for (size_t n = 0; n < taskCount; ++n)
    {
        if (min > m_mins[n])
            min = m_mins[n];
        if (max < m_maxs[n])
            max = m_maxs[n];
        meanSum += m_meanSums[n];
        pixCount += m_pixCounts[n];
    }

    // It could happen there is no ROI in metadata thus zero pixels
    if (pixCount > 0)
    {
        *m_min = min;
        *m_max = max;
        *m_mean = (uint16_t)(meanSum / pixCount);
    }
    else
    {
        *m_min = 0;
        *m_max = 0;
        *m_mean = 0;
    }
}
