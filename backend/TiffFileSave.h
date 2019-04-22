#pragma once
#ifndef _TIFF_FILE_SAVE_H
#define _TIFF_FILE_SAVE_H

/* Local */
#include "FileSave.h"

// Forward declaration for md_frame that satisfies compiler (taken from pvcam.h)
struct md_frame;
typedef struct md_frame md_frame;

// Forward declaration for TIFF that satisfies compiler (taken from tiffio.h)
struct tiff;
typedef struct tiff TIFF;

namespace pm {

class TiffFileSave final : public FileSave
{
public:
    TiffFileSave(const std::string& fileName, PrdHeader& header);
    virtual ~TiffFileSave();

    TiffFileSave() = delete;
    TiffFileSave(const TiffFileSave&) = delete;
    TiffFileSave(TiffFileSave&&) = delete;
    TiffFileSave& operator=(const TiffFileSave&) = delete;
    TiffFileSave& operator=(TiffFileSave&&) = delete;

public: // From File
    virtual bool Open() override;
    virtual bool IsOpen() const override;
    virtual void Close() override;

public: // From FileSave
    virtual bool WriteFrame(const void* metaData, const void* extDynMetaData,
            const void* rawData) override;
    virtual bool WriteFrame(const Frame& frame, uint32_t expTime) override;

public:
    static std::string GetImageDesc(const PrdHeader& prdHeader,
            const void* prdMeta, const md_frame* pvcamMeta);

private:
    TIFF* m_file;
    md_frame* m_frameMeta;
    void* m_frameRecomposed;
    const size_t m_frameRecomposedBytes;
};

} // namespace pm

#endif /* _TIFF_FILE_SAVE_H */
