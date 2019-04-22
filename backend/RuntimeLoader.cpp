#include "RuntimeLoader.h"

/* System */
#include <algorithm>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
    // Included as last to not polute other sources with _GNU_SOURCE macro
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE // Needed for dlinfo
    #endif
    #include <dlfcn.h> // dlinfo
    #include <link.h> // struct link_map
#else
    #error OS Not supported
#endif

pm::RuntimeLoader::RuntimeLoader()
    : m_handle(nullptr),
    m_fileName(),
    m_filePath(),
    m_fullFileName()
{
}

pm::RuntimeLoader::~RuntimeLoader()
{
}

const std::string& pm::RuntimeLoader::GetFileName() const
{
    return m_fileName;
}

const std::string& pm::RuntimeLoader::GetFilePath() const
{
    return m_filePath;
}

const std::string& pm::RuntimeLoader::GetFullFileName() const
{
    return m_fullFileName;
}

void pm::RuntimeLoader::Load(const char* fileName)
{
    if (m_handle != nullptr)
    {
        throw Exception("Library already loaded: '" + m_fullFileName + "'");
    }

#if defined(_WIN32)
    m_handle = (void*)LoadLibraryA(fileName);
#elif defined(__linux__) || defined(__APPLE__)
    m_handle = dlopen(fileName, RTLD_LAZY);
#endif

    if (m_handle == nullptr)
    {
        throw Exception("Unable to load library '" + std::string(fileName)
                + "' (" + GetErrorString() + ")");
    }

    std::string fullFileName;
#if defined(_WIN32)
    char buffer[4096];
    if (GetModuleFileNameA((HMODULE)m_handle, buffer, sizeof(buffer)) != 0)
    {
        fullFileName = buffer;
    }
#elif defined(__linux__) || defined(__APPLE__)
    struct link_map* linkMap = nullptr;
    if (dlinfo(m_handle, RTLD_DI_LINKMAP, &linkMap) == 0 && linkMap)
    {
        fullFileName = linkMap->l_name;
    }
#endif
    else
    {
        throw Exception("Failed to get full path for library '"
                + std::string(fileName) + "' (" + GetErrorString() + ")");
    }

    auto it = std::find_if(fullFileName.crbegin(), fullFileName.crend(),
            [](char c) -> bool {
#if defined(_WIN32)
                return c == '\\' || c == '/';
#elif defined(__linux__) || defined(__APPLE__)
                return c == '/';
#endif
            }).base();

    m_fileName.assign(it, fullFileName.cend());
    m_filePath.assign(fullFileName.cbegin(), it);
    m_fullFileName = fullFileName;
}

bool pm::RuntimeLoader::IsLoaded() const
{
    return (m_handle != nullptr);
}

void pm::RuntimeLoader::Unload()
{
    if (m_handle == nullptr)
    {
        throw Exception("No library loaded");
    }

    std::string error;

#if defined(_WIN32)
    if (FreeLibrary((HMODULE)m_handle) == FALSE)
#elif defined(__linux__) || defined(__APPLE__)
    if (dlclose(m_handle) != 0)
#endif
    {
        error = "Failed to unload library '" + m_fullFileName + "' ("
                + GetErrorString() + ")";
    }

    m_handle = nullptr;
    m_fileName.clear();
    m_filePath.clear();
    m_fullFileName.clear();

    if (!error.empty())
    {
        throw Exception(error);
    }
}

void* pm::RuntimeLoader::LoadSymbol(const char* symbolName, bool silent)
{
    if (m_handle == nullptr)
    {
        throw Exception("No library loaded");
    }

    void* symbol;

#if defined(_WIN32)
    symbol = (void*)GetProcAddress((HMODULE)m_handle, symbolName);
#elif defined(__linux__) || defined(__APPLE__)
    symbol = (void*)dlsym(m_handle, symbolName);
#endif

    if (!symbol && !silent)
    {
        throw Exception("Failed to load symbol '"
                + std::string(symbolName) + "' from '" + m_fullFileName + "'");
    }

    return symbol;
}

#if defined(_WIN32)
std::string _GetWinErrorString(DWORD errorCode)
{
    char errBuf[4096];
    const DWORD formatFlags = FORMAT_MESSAGE_FROM_SYSTEM
       // Function could fails without this - we cannot pass insertion parameters
       | FORMAT_MESSAGE_IGNORE_INSERTS;
    if (FormatMessageA(formatFlags, NULL, errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errBuf,
                sizeof(errBuf) / sizeof(errBuf[0]), NULL))
    {
        return errBuf;
    }
    return "Unknown error";
}
#endif // _WIN32

std::string pm::RuntimeLoader::GetErrorString()
{
#if defined(_WIN32)
    const DWORD errorCode = GetLastError();
    std::string err(_GetWinErrorString(errorCode));
#elif defined(__linux__) || defined(__APPLE__)
    const char* errorStr = dlerror();
    std::string err((errorStr != NULL) ? errorStr : "No error");
#endif
    err.erase(std::remove(err.begin(), err.end(), '\r'), err.end());
    err.erase(std::remove(err.begin(), err.end(), '\n'), err.end());
    return err;
}
