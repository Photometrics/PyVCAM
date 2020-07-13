#pragma once
#ifndef _TIMER_H
#define _TIMER_H

// Uncomment to use C++11 chrono instead QueryPerformanceCounter on Windows
//#define USE_CHRONO

/* System */
#if !defined(_MSC_VER) || defined(USE_CHRONO)
#include <chrono>
#endif

namespace pm {

class Timer
{
public:
    Timer();

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // Makes the timer start over counting from 0.0 seconds
    void Reset();
    // Number of seconds (to very high resolution) elapsed since last reset
    double Seconds() const;
    // Number of milliseconds (to very high resolution) elapsed since last reset
    double Milliseconds() const
    { return Seconds() * 1000.0; }
    // Number of microseconds (to very high resolution) elapsed since last reset
    double Microseconds() const
    { return Milliseconds() * 1000.0; }

private:
#if defined(_MSC_VER) && !defined(USE_CHRONO)
    double m_freq;
    long long m_time0;
#else
    std::chrono::high_resolution_clock::time_point m_time0;
#endif
};

} // namespace pm

#endif /* _TIMER_H */
