#ifndef PM_UNIQUE_THREAD_POOL_H
#define PM_UNIQUE_THREAD_POOL_H

/* Local */
#include "ThreadPool.h"

/* System */
#include <mutex> // std::once_flag

namespace pm {

class UniqueThreadPool final
{
public:
    // Creates singleton instance
    static UniqueThreadPool& Get();

private:
    static UniqueThreadPool* m_instance;
    static std::once_flag m_onceFlag;

public:
    std::shared_ptr<ThreadPool> GetPool() const;

private:
    UniqueThreadPool();
    UniqueThreadPool(const UniqueThreadPool&) = delete;
    UniqueThreadPool(UniqueThreadPool&&) = delete;
    void operator=(const UniqueThreadPool&) = delete;
    void operator=(UniqueThreadPool&&) = delete;
    ~UniqueThreadPool();

private:
    std::shared_ptr<ThreadPool> m_threadPool;
};

} // namespace pm

#endif // PM_UNIQUE_THREAD_POOL_H
