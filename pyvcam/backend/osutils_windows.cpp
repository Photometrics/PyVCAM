#if defined(_WIN32)

#include "osutils.h"

/* System */
#include <algorithm>
#include <map>
#include <sstream>
#include <Windows.h>

// Local
#include "Log.h"


size_t pm::GetTotalVirtualMemBytes()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t) status.ullTotalVirtual;
}

size_t pm::GetAvailVirtualMemBytes()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t) status.ullAvailVirtual;
}

size_t pm::GetTotalPhysicalMemBytes()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t) status.ullTotalPhys;
}

size_t pm::GetAvailPhysicalMemBytes()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t) status.ullAvailPhys;
}

std::vector<std::string> pm::GetFiles(const std::string& dir, const std::string& ext)
{
    std::vector<std::string> files;

    WIN32_FIND_DATAA fdFile;
    const std::string filter = dir + "/*" + ext;
    HANDLE hFind = FindFirstFileA(filter.c_str(), &fdFile);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string fullFileName = dir + "/" + fdFile.cFileName;
            /* Ignore folders */
            if ((fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                continue;
            /* Check file name ends with extension */
            const size_t extPos = fullFileName.rfind(ext);
            if (extPos == std::string::npos
                || extPos + ext.length() != fullFileName.length())
                continue;
            files.push_back(fullFileName);
        }
        while (FindNextFileA(hFind, &fdFile));

        FindClose(hFind);
    }

    return files;
}

// Increases the priority of the current thread to THREAD_PRIORITY_ABOVE_NORMAL
// if its priority is not already as high as that
void pm::SetCurrentThreadPriorityAboveNormal()
{
    DWORD thread_id = GetCurrentThreadId();
    HANDLE thread_handle = GetCurrentThread();
    int old_priority = GetThreadPriority(thread_handle);
    int new_priority = THREAD_PRIORITY_ABOVE_NORMAL;

    if (old_priority >= new_priority &&
        old_priority != THREAD_PRIORITY_ERROR_RETURN)
    {
        // thread priority is already higher than normal
        return;
    }

    if (!SetThreadPriority(thread_handle, new_priority))
    {
        unsigned int error_code = GetLastError();
        Log::LogE(
            "Failed to increase the priority of thread ID %u: error code %u\n",
            (unsigned int) thread_id,
            error_code);
        return;
    }

    pm::Log::LogD("Changed priority of thread ID %u from %d to %d\n",
        (unsigned int) thread_id,
        (int) old_priority,
        (int) new_priority);
}

#endif // defined(_WIN32)
