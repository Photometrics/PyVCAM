#pragma once
#ifndef _PRD_FILE_SAVE_H
#define _PRD_FILE_SAVE_H

/* System */
#include <fstream>

/* Local */
#include "FileSave.h"

namespace pm {

class PrdFileSave final : public FileSave
{
public:
    PrdFileSave(const std::string& fileName, const PrdHeader& header);
    virtual ~PrdFileSave();

    PrdFileSave() = delete;
    PrdFileSave(const PrdFileSave&) = delete;
    PrdFileSave(PrdFileSave&&) = delete;
    PrdFileSave& operator=(const PrdFileSave&) = delete;
    PrdFileSave& operator=(PrdFileSave&&) = delete;

public: // From File
    virtual bool Open() override;
    virtual bool IsOpen() const override;
    virtual void Close() override;

public: // From FileSave
    virtual bool WriteFrame(const void* metaData, const void* extDynMetaData,
            const void* rawData) override;
    virtual bool WriteFrame(const Frame& frame, uint32_t expTime) override;

private:
    std::ofstream m_file;
};

} // namespace pm

#endif /* _PRD_FILE_SAVE_H */
