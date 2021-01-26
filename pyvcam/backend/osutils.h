#pragma once
#ifndef _UTILS_H
#define _UTILS_H

/* System */
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

/* Helper macro that makes multi-line function-like macros more safe
   Usage example:
    #define MULTI_LINE_FUNC_LIKE_MACRO(a,b,c) do {\
        <code>
    } ONCE
*/
#if defined(_WIN32) || defined(_WIN64)
  #define ONCE\
    __pragma(warning(push))\
    __pragma(warning(disable:4127))\
    while (0)\
    __pragma(warning(pop))
#else
  #define ONCE while (0)
#endif

/* Platform independent macro to avoid compiler warnings for unreferenced
   function parameters. It has the same effect as Win32 UNREFERENCED_PARAMETER
   Usage example:
       void do_magic(int count)
       {
           UNUSED(count);
       }
*/
#define UNUSED(expr) do { (void)(expr); } ONCE

namespace pm {

size_t GetTotalVirtualMemBytes();
size_t GetAvailVirtualMemBytes();
size_t GetTotalPhysicalMemBytes();
size_t GetAvailPhysicalMemBytes();

// Return a list of file names in given folder with given extension
std::vector<std::string> GetFiles(const std::string& dir, const std::string& ext);

// Increases the priority of the current thread to THREAD_PRIORITY_ABOVE_NORMAL
// if its priority is not already as high as that
void SetCurrentThreadPriorityAboveNormal();

} // namespace pm

#endif /* _UTILS_H */
