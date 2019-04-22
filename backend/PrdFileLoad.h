#pragma once
#ifndef _PRD_FILE_LOAD_H
#define _PRD_FILE_LOAD_H

/* System */
#include <fstream>

/* Local */
#include "FileLoad.h"

namespace pm {

class PrdFileLoad final : public FileLoad
{
public:
    // The header structure is filled in Open method
    PrdFileLoad(const std::string& fileName, PrdHeader& header);
    virtual ~PrdFileLoad();

    PrdFileLoad() = delete;
    PrdFileLoad(const PrdFileLoad&) = delete;
    PrdFileLoad(PrdFileLoad&&) = delete;
    PrdFileLoad& operator=(const PrdFileLoad&) = delete;
    PrdFileLoad& operator=(PrdFileLoad&&) = delete;

public: // From File
    // Fills the PrdHeader structure given in constructor
    virtual bool Open() override;
    virtual bool IsOpen() const override;
    virtual void Close() override;

public: // From FileLoad
    virtual bool ReadFrame(const void** metaData, const void** extDynMetaData,
            const void** rawData) override;

private:
    std::ifstream m_file;
};

} // namespace pm

#endif /* _PRD_FILE_LOAD_H */
