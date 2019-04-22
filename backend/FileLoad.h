#pragma once
#ifndef _FILE_LOAD_H
#define _FILE_LOAD_H

/* Local */
#include "File.h"
#include "PrdFileFormat.h"

namespace pm {

class FileLoad : public File
{
public:
    FileLoad(const std::string& fileName, PrdHeader& header);
    virtual ~FileLoad();

    FileLoad() = delete;
    FileLoad(const FileLoad&) = delete;
    FileLoad(FileLoad&&) = delete;
    FileLoad& operator=(const FileLoad&) = delete;
    FileLoad& operator=(FileLoad&&) = delete;

public:
    PrdHeader& GetHeader() const;

public: // From File
    virtual void Close() override;

public:
    // Next frame is read out of the file

    // metaData and rawData have to be pre-allocated according to info in
    // PrdHeader given to constructor. extDynMetaData is auto-detected during
    // reading from file, memory is allocated and filled. The memory is owned
    // by this class and can be released or reallocated in each ReadFrame call.
    virtual bool ReadFrame(const void** metaData, const void** extDynMetaData,
            const void** rawData);

protected:
    PrdHeader& m_header;
    size_t m_rawDataBytes;

    void* m_metaData;
    void* m_extDynMetaData;
    void* m_rawData;
};

} // namespace pm

#endif /* _FILE_LOAD_H */
