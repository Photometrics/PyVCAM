#include "Timer.h"

#if defined(_MSC_VER) && !defined(USE_CHRONO)
#include <Windows.h>

pm::Timer::Timer()
{
    LARGE_INTEGER li;
    // Returns the frequency in counts per second
    QueryPerformanceFrequency(&li);
    m_freq = (double)li.QuadPart;

    Reset();
}

void pm::Timer::Reset()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    m_time0 = li.QuadPart;
}

double pm::Timer::Seconds() const
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (double)(li.QuadPart - m_time0) / m_freq;
}

#else /* defined(_MSC_VER) && !defined(USE_CHRONO) */

pm::Timer::Timer()
{
    Reset();
}

void pm::Timer::Reset()
{
    m_time0 = std::chrono::high_resolution_clock::now();
}

double pm::Timer::Seconds() const
{
    const std::chrono::high_resolution_clock::time_point now =
        std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<double>>(now - m_time0).count();
}

#endif /* defined(_MSC_VER) && !defined(USE_CHRONO) */
