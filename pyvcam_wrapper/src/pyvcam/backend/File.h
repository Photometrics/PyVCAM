#pragma once
#ifndef _FILE_H
#define _FILE_H

/* System */
#include <cstdint>
#include <string>

namespace pm {

class File
{
public:
    File(const std::string& fileName);
    virtual ~File();

    File() = delete;
    File(const File&) = delete;
    File(File&&) = delete;
    File& operator=(const File&) = delete;
    File& operator=(File&&) = delete;

public:
    const std::string& GetFileName() const;

public:
    virtual bool Open() = 0;
    virtual bool IsOpen() const = 0;
    virtual void Close() = 0;

protected:
    const std::string m_fileName;
    uint32_t m_frameIndex;
};

} // namespace pm

#endif /* _FILE_H */
