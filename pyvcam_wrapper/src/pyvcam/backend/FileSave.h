#pragma once
#ifndef _FILE_SAVE_H
#define _FILE_SAVE_H

/* Local */
#include "File.h"
#include "Frame.h"
#include "PrdFileFormat.h"

namespace pm {

class FileSave : public File
{
public:
    FileSave(const std::string& fileName, const PrdHeader& header);
    virtual ~FileSave();

    FileSave() = delete;
    FileSave(const FileSave&) = delete;
    FileSave(FileSave&&) = delete;
    FileSave& operator=(const FileSave&) = delete;
    FileSave& operator=(FileSave&&) = delete;

public:
    const PrdHeader& GetHeader() const;

public: // From File
    virtual void Close() override;

public:
    // New frame is added at end of the file

    virtual bool WriteFrame(const void* metaData, const void* extDynMetaData,
            const void* rawData);
    virtual bool WriteFrame(const Frame& frame, uint32_t expTime);

private:
    bool UpdateFrameExtMetaData(const Frame& frame);
    bool UpdateFrameExtDynMetaData(const Frame& frame);

    uint32_t GetExtMetaDataSizeInBytes(const pm::Frame& frame);

protected:
    PrdHeader m_header;

    const size_t m_width;
    const size_t m_height;
    const size_t m_rawDataBytes;

    void* m_framePrdMetaData;
    void* m_framePrdExtDynMetaData;
    uint32_t m_framePrdExtDynMetaDataBytes;

private:
    // Zero until first frame comes, then set to orig. size from header
    uint32_t m_frameOrigSizeOfPrdMetaDataStruct;

    uint32_t m_framePrdMetaDataExtFlags;
    uint32_t m_trajectoriesBytes;
};

} // namespace pm

#endif /* _FILE_SAVE_H */
