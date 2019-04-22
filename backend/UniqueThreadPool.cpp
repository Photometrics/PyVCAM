#include "UniqueThreadPool.h"

pm::UniqueThreadPool* pm::UniqueThreadPool::m_instance = nullptr;
std::once_flag pm::UniqueThreadPool::m_onceFlag;

pm::UniqueThreadPool& pm::UniqueThreadPool::Get()
{
    std::call_once(m_onceFlag, []() {
        m_instance = new(std::nothrow) UniqueThreadPool();
    });
    return *m_instance;

    // TODO: Current solution causes memory leak of m_instance at app. exit.
    //       When changed to smart pointer or local static variable (as shown
    //       below) it causes a hang in m_thread->join even though the thread
    //       function exits gracefully. The worst thing is that this happens
    //       only in case the app. is closed right after opening.
    //       It seems that wxWidgets code initiates memory leak dumps because
    //       other apps using the same pattern do not report any leak.
    //       Retest later with MSVS 2015.

    // This is thread-safe in C++11, but on Windows only since MSVS 2015
    //static UniqueThreadPool instance;
    //return instance;
}

std::shared_ptr<pm::ThreadPool> pm::UniqueThreadPool::GetPool() const
{
    return m_threadPool;
}

pm::UniqueThreadPool::UniqueThreadPool()
    : m_threadPool(std::make_shared<ThreadPool>(
            std::max(1u, std::thread::hardware_concurrency())))
{
}

pm::UniqueThreadPool::~UniqueThreadPool()
{
    m_threadPool = nullptr;
}
