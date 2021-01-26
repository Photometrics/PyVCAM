#if defined(__linux__)

/* System */
#include <algorithm>
#include <map>
#include <sstream>

#include <dirent.h> // opendir, closedir, readdir
#include <fstream> // std::ifstream
#include <limits> // std::numeric_limits
#include <unistd.h> // stat
#include <sys/stat.h> // stat
#include <sys/types.h> // opendir, closedir, stat

// Local
#include "osutils.h"

namespace pm {
bool GetProcMemInfo(size_t* total, size_t* avail)
{
    std::ifstream file("/proc/meminfo");
    if (!file)
        return false;

    // Both function arguments are optional, we don't search those that are null
    bool foundTotal = (total == nullptr);
    bool foundAvail = (avail == nullptr);

    size_t tmpAvail = 0; // Used in case MemAvailable not supported
    bool foundAvail1 = false;
    bool foundAvail2 = false;
    bool foundAvail3 = false;
    bool foundAvail4 = false;

    std::string token;
    size_t mem;

    while (!foundTotal || !foundAvail) {
        if (!(file >> token))
            return false;
        if (!(file >> mem))
            return false;
        // Ignore rest of the line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (!foundTotal) {
            if (token == "MemTotal:") {
                *total = mem * 1024; // Convert kB to bytes
                foundTotal = true;
                continue;
            }
        }
        if (!foundAvail) {
            if (token == "MemAvailable:") {
                // MemAvailable is available with newer kernels only.
                *avail = mem * 1024; // Convert kB to bytes
                foundAvail = true;
                continue;
            }
            else {
                // For older kernels we sum 1-MemFree, 2-Active(file),
                // 3-Inactive(file) and 4-SReclaimable.
                // Value of "low" watermarks from /proc/zoneinfo is ignored.
                if (token == "MemFree:") {
                    tmpAvail += mem * 1024; // Convert kB to bytes
                    foundAvail1 = true;
                }
                else if (token == "Active(file):") {
                    tmpAvail += mem * 1024; // Convert kB to bytes
                    foundAvail2 = true;
                }
                else if (token == "Inactive(file):") {
                    tmpAvail += mem * 1024; // Convert kB to bytes
                    foundAvail3 = true;
                }
                else if (token == "SReclaimable:") {
                    tmpAvail += mem * 1024; // Convert kB to bytes
                    foundAvail4 = true;
                }
                foundAvail = foundAvail1 && foundAvail2 && foundAvail3 && foundAvail4;
                if (foundAvail) {
                    *avail = tmpAvail;
                    continue;
                }
            }
        }
    }
    return foundTotal && foundAvail;
}
} // namespace pm

size_t pm::GetTotalPhysicalMemBytes()
{
    size_t total;
    const bool ok = pm::GetProcMemInfo(&total, nullptr);
    return (ok) ? total : 0;
}

size_t pm::GetAvailPhysicalMemBytes()
{
    size_t avail;
    const bool ok = pm::GetProcMemInfo(nullptr, &avail);
    return (ok) ? avail : 0;
}

std::vector<std::string> pm::GetFiles(const std::string& dir, const std::string& ext)
{
    std::vector<std::string> files;
    DIR* dr = opendir(dir.c_str());
    if (dr)
    {
        struct dirent* ent;
        struct stat st;
        while ((ent = readdir(dr)) != NULL)
        {
            const std::string fileName = ent->d_name;
            const std::string fullFileName = dir + "/" + fileName;
            /* Ignore folders */
            if (stat(fullFileName.c_str(), &st) != 0)
                continue;
            if (st.st_mode & S_IFDIR)
                continue;
            /* Check file extension */
            const size_t extPos = fileName.rfind(ext);
            if (extPos == std::string::npos
                    || extPos + ext.length() != fileName.length())
                continue;
            files.push_back(fullFileName);
        }
        closedir(dr);
    }

    return files;
}

#endif // defined(__linux__)
