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

// Converts string to integral number of given type
template<typename T>
bool StrToNumber(const std::string& str,
        typename std::enable_if<
            std::is_integral<T>::value && std::is_signed<T>::value,
            T>::type& number)
{
    try {
        size_t idx;
        const long long nr = std::stoll(str, &idx);
        if (idx == str.length()
                && nr >= (long long)(std::numeric_limits<T>::min)()
                && nr <= (long long)(std::numeric_limits<T>::max)())
        {
            number = (T)nr;
            return true;
        }
    }
    catch(...) {};
    return false;
}

// Converts string to integral number of given type
template<typename T>
bool StrToNumber(const std::string& str,
        typename std::enable_if<
            std::is_integral<T>::value && std::is_unsigned<T>::value,
            T>::type& number)
{
    try {
        size_t idx;
        const unsigned long long nr = std::stoull(str, &idx);
        if (idx == str.length()
                && nr >= (unsigned long long)(std::numeric_limits<T>::min)()
                && nr <= (unsigned long long)(std::numeric_limits<T>::max)())
        {
            number = (T)nr;
            return true;
        }
    }
    catch(...) {};
    return false;
}

// Converts string to boolean value
bool StrToBool(const std::string& str, bool& value);

// Splits string into sub-strings separated by given delimiter
std::vector<std::string> SplitString(const std::string& string, char delimiter);

// Joins strings from vector into one string using given delimiter
std::string JoinStrings(const std::vector<std::string>& strings, char delimiter);

size_t GetTotalPhysicalMemBytes();
size_t GetAvailPhysicalMemBytes();

// Return a list of file names in given folder with given extension
std::vector<std::string> GetFiles(const std::string& dir, const std::string& ext);

// Converts fixed-point to real number
template<typename R, typename U>
typename std::enable_if<std::is_floating_point<R>::value, R>::type
    FixedPointToReal(uint8_t integralBits, uint8_t fractionBits,
        typename std::enable_if<
            std::is_integral<U>::value && std::is_unsigned<U>::value,
            U>::type value)
{
    const uint64_t val64 = static_cast<uint64_t>(value);

    const uint64_t intSteps = 1ULL << integralBits;
    const uint64_t fractSteps = 1ULL << fractionBits;

    const uint64_t intMask = intSteps - 1ULL;
    const uint64_t fractMask = fractSteps - 1ULL;

    const R intPart = static_cast<R>((val64 >> fractionBits) & intMask);
    const R fractPart = (val64 & fractMask) / static_cast<R>(fractSteps);

    const R real = intPart + fractPart;
    return real;
}

// Converts real number to fixed-point
template<typename R, typename U>
        typename std::enable_if<
            std::is_integral<U>::value && std::is_unsigned<U>::value,
            U>::type
    RealToFixedPoint(uint8_t integralBits, uint8_t fractionBits,
        typename std::enable_if<std::is_floating_point<R>::value, R>::type value)
{
    //const uint64_t intSteps = 1ULL << integralBits;
    const uint64_t fractSteps = 1ULL << fractionBits;
    const uint64_t steps = 1ULL << (integralBits + fractionBits);

    const uint64_t mask = steps - 1ULL;

    const U fixedPoint = static_cast<U>(value * fractSteps) & mask;
    return fixedPoint;
}

void SetCurrentThreadPriorityAboveNormal();

} // namespace pm

#endif /* _UTILS_H */
