#pragma once
#ifndef PM_RUNTIME_LOADER_H
#define PM_RUNTIME_LOADER_H

/* System */
#include <stdexcept>
#include <string>

namespace pm {

class RuntimeLoader
{
public:
    class Exception : public std::runtime_error
    {
    public:
        explicit Exception(const std::string& what) : std::runtime_error(what) {}
        explicit Exception(const char* what) : std::runtime_error(what) {}
    };

public:
    RuntimeLoader();
    virtual ~RuntimeLoader();

    RuntimeLoader(const RuntimeLoader&) = delete;
    RuntimeLoader(RuntimeLoader&&) = delete;
    RuntimeLoader& operator=(const RuntimeLoader&) = delete;
    RuntimeLoader& operator=(RuntimeLoader&&) = delete;

public:
    /// @brief Returns library file name without path.
    const std::string& GetFileName() const;
    /// @brief Returns library absolute path without file name.
    const std::string& GetFilePath() const;
    /// @brief Returns library file name with absolute path.
    const std::string& GetFullFileName() const;

public:
    /// @brief Loads library given as file name with absolute, relative or no path.
    /// @throw RuntimeLoader::Exception with error message for logging.
    virtual void Load(const char* fileName);
    /// @brief Says whether some library is loaded or not.
    virtual bool IsLoaded() const;
    /// @brief Unloads already loaded library.
    /// @throw RuntimeLoader::Exception with error message for logging.
    virtual void Unload();

    /// @brief In sub-class loads all library symbols known at compile time.
    /// @throw RuntimeLoader::Exception only if library not loaded or if some
    /// symbol not found and @a silent is false.
    virtual bool LoadSymbols(bool silent = false) = 0;

protected:
    /// @throw RuntimeLoader::Exception only if library not loaded or if symbol
    /// not found and @a silent is false.
    void* LoadSymbol(const char* symbolName, bool silent);

    /// @brief Returns a string for last error related to runtime library operations.
    static std::string GetErrorString();

protected:
    void* m_handle;
    std::string m_fileName;
    std::string m_filePath;
    std::string m_fullFileName;
};

} // namespace pm

#endif /* PM_RUNTIME_LOADER_H */
