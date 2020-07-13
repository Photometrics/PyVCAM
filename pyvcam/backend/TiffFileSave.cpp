#include "TiffFileSave.h"

/* System */
#include <cstring>
#include <limits>
#include <map>
#include <sstream>

/* tinyTiff */
#include "tinytiffwriter.h"

/* PVCAM */
#include <master.h>
#include <pvcam.h>

/* Local */
#include "Log.h"

pm::TiffFileSave::TiffFileSave(const std::string& fileName, PrdHeader& header)
    : FileSave(fileName, header),
    m_file(nullptr),
    m_frameMeta(nullptr),
    m_frameRecomposed(nullptr),
    m_frameRecomposedBytes(sizeof(uint16_t) * m_width * m_height)
{
}

pm::TiffFileSave::~TiffFileSave()
{
    if (IsOpen())
        Close();

    delete [] (uint8_t*)m_frameRecomposed;
    if (m_frameMeta)
        pl_md_release_frame_struct(m_frameMeta);
}

bool pm::TiffFileSave::Open()
{
    if (IsOpen())
        return true;

#if SIZE_MAX > UINT32_MAX
    if (m_rawDataBytes > std::numeric_limits<uint32_t>::max())
    {
        Log::LogE("TIFF format is unable to store more than 4GB raw data");
        return false;
    }
#endif

    // Open TIFF file with proper size and bit depth
    m_file = TinyTIFFWriter_open(m_fileName.c_str(), 16, (uint32_t)m_width, (uint32_t)m_height);
    if (!m_file)
        return false;

    m_frameIndex = 0;

    return IsOpen();
}

bool pm::TiffFileSave::IsOpen() const
{
    return !!m_file;
}

void pm::TiffFileSave::Close()
{
    if (m_header.frameCount != m_frameIndex)
    {
        // TODO: Update frame count in TIFF file
        Log::LogE("File does not contain declared number of frame."
            " Automatic correction not implemented yet");

        m_header.frameCount = m_frameIndex;

        //m_file.seekp(0);
        //m_file.write((char*)&m_header, sizeof(PrdHeader));
        //m_file.seekp(0, std::ios_base::end);
    }

    // Close TIFF file, writing metadata before closing
    TinyTIFFWriter_close(m_file);
    m_file = nullptr;

    FileSave::Close();
}

bool pm::TiffFileSave::WriteFrame(const void* metaData,
        const void* extDynMetaData, const void* rawData)
{
    if (!FileSave::WriteFrame(metaData, extDynMetaData, rawData))
        return false;

    // Recompose the black-filled frame with metadata if any
    void* tiffData;
    size_t tiffDataBytes;
    if (m_header.version >= PRD_VERSION_0_3
            && (m_header.flags & PRD_FLAG_HAS_METADATA))
    {
        // Allocate md_frame only once
        if (!m_frameMeta)
        {
            if (PV_OK != pl_md_create_frame_struct(&m_frameMeta,
                        (void*)rawData, (uns32)m_rawDataBytes))
                return false;
            // Allocate buffer for recomposed frame only once
            if (!m_frameRecomposed)
            {
                m_frameRecomposed = new(std::nothrow) uint8_t[m_frameRecomposedBytes];
                if (!m_frameRecomposed)
                    return false;
            }
        }

        // Fill the frame with black pixels
        memset(m_frameRecomposed, 0, m_frameRecomposedBytes);

        // Decode metadata and recompose the frame
        if (PV_OK != pl_md_frame_decode(m_frameMeta, (void*)rawData,
                    (uns32)m_rawDataBytes))
            return false;

        // While streaming we recompose implied ROI only which starts at [0, 0]
        const uint16_t xOff = 0;
        const uint16_t yOff = 0;

        if (PV_OK != pl_md_frame_recompose(m_frameRecomposed, xOff, yOff,
                    (uns16)m_width, (uns16)m_height, m_frameMeta))
            return false;

        tiffDataBytes = m_frameRecomposedBytes;
        tiffData = m_frameRecomposed;
    }
    else
    {
        tiffData = const_cast<void*>(rawData);
        tiffDataBytes = m_rawDataBytes;
    }

    // Build up the metadata description
    m_tiffDesc = GetImageDesc(m_header, metaData, m_frameMeta);

    // Skip the metadata at the beginning of the file
    TinyTIFFWriter_writeImage(m_file, tiffData);

    m_frameIndex++;
    return true;
}

bool pm::TiffFileSave::WriteFrame(const Frame& frame, uint32_t expTime)
{
    if (!FileSave::WriteFrame(frame, expTime))
        return false;

    // TODO: Utilize possibly decoded, recomposed and "colorized" Frame
    //       to improve performance
    return WriteFrame(m_framePrdMetaData, m_framePrdExtDynMetaData, frame.GetData());
}

std::string pm::TiffFileSave::GetImageDesc(const PrdHeader& prdHeader,
        const void* prdMeta, const md_frame* pvcamMeta)
{
    if (!prdMeta)
        return "";

    std::ostringstream imageDesc;

    auto prdMetaData = static_cast<const PrdMetaData*>(prdMeta);

    imageDesc << "bitDepth=" << prdHeader.bitDepth;
    if (prdHeader.version >= PRD_VERSION_0_1)
    {
        static std::map<uint32_t, const char*> expResUnit =
        {
            { PRD_EXP_RES_US, "us" },
            { PRD_EXP_RES_MS, "ms" },
            { PRD_EXP_RES_S, "s" }
        };
        imageDesc
            << "\nregion=[" << prdHeader.region.s1 << "," << prdHeader.region.s2
                << "," << prdHeader.region.sbin << "," << prdHeader.region.p1
                << "," << prdHeader.region.p2 << "," << prdHeader.region.pbin << "]"
            << "\nframeNr=" << prdMetaData->frameNumber
            << "\nreadoutTime=" << prdMetaData->readoutTime << "us"
            << "\nexpTime=" << prdMetaData->exposureTime
                << ((expResUnit.count(prdHeader.exposureResolution) > 0)
                        ? expResUnit[prdHeader.exposureResolution]
                        : "<unknown unit>");
    }
    if (prdHeader.version >= PRD_VERSION_0_2)
    {
        uint64_t bofTime = prdMetaData->bofTime;
        uint64_t eofTime = prdMetaData->eofTime;
        if (prdHeader.version >= PRD_VERSION_0_4)
        {
            bofTime |= (uint64_t)prdMetaData->bofTimeHigh << 32;
            eofTime |= (uint64_t)prdMetaData->eofTimeHigh << 32;
        }
        imageDesc
            << "\nbofTime=" << bofTime << "us"
            << "\neofTime=" << eofTime << "us";
    }
    if (prdHeader.version >= PRD_VERSION_0_3)
    {
        // Cast 8bit values to 16bit, otherwise ostream processes it as char
        imageDesc
            << "\nroiCount=" << prdMetaData->roiCount
            << "\ncolorMask=" << (uint16_t)prdHeader.colorMask
            << "\nflags=0x" << std::hex << (uint16_t)prdHeader.flags << std::dec;
    }

    if (pvcamMeta && prdHeader.version >= PRD_VERSION_0_3
            && (prdHeader.flags & PRD_FLAG_HAS_METADATA))
    {
        const rgn_type& irgn = pvcamMeta->impliedRoi;
        // uns8 type is handled as underlying char by stream, cast it to uint16_t
        imageDesc
            << "\nmeta.header.version=" << (uint16_t)pvcamMeta->header->version
            << "\nmeta.header.frameNr=" << pvcamMeta->header->frameNr
            << "\nmeta.header.roiCount=" << pvcamMeta->header->roiCount
            << "\nmeta.header.timeBof=" << pvcamMeta->header->timestampBOF
            << "\nmeta.header.timeEof=" << pvcamMeta->header->timestampEOF
            << "\nmeta.header.timeResNs=" << pvcamMeta->header->timestampResNs
            << "\nmeta.header.expTime=" << pvcamMeta->header->exposureTime
            << "\nmeta.header.expTimeResNs=" << pvcamMeta->header->exposureTimeResNs
            << "\nmeta.header.roiTimeResNs=" << pvcamMeta->header->roiTimestampResNs
            << "\nmeta.header.bitDepth=" << (uint16_t)pvcamMeta->header->bitDepth
            << "\nmeta.header.colorMask=" << (uint16_t)pvcamMeta->header->colorMask
            << "\nmeta.header.flags=" << (uint16_t)pvcamMeta->header->flags
            << "\nmeta.header.extMdSize=" << pvcamMeta->header->extendedMdSize
            << "\nmeta.extMdSize=" << pvcamMeta->extMdDataSize
            << "\nmeta.impliedRoi=[" << irgn.s1 << "," << irgn.s2 << "," << irgn.sbin
                << "," << irgn.p1 << "," << irgn.p2 << "," << irgn.pbin << "]"
            << "\nmeta.roiCapacity=" << pvcamMeta->roiCapacity
            << "\nmeta.roiCount=" << pvcamMeta->roiCount;
        for (int n = 0; n < pvcamMeta->roiCount; ++n)
        {
            const md_frame_roi& roi = pvcamMeta->roiArray[n];
            const auto roiHdr = roi.header;
            if (roiHdr->flags & PL_MD_ROI_FLAG_INVALID)
                continue; // Skip invalid regions
            const rgn_type& rgn = roiHdr->roi;
            imageDesc
                << "\nmeta.roi[" << n << "].header.roiNr=" << roiHdr->roiNr;
            if (pvcamMeta->header->flags & PL_MD_FRAME_FLAG_ROI_TS_SUPPORTED)
            {
                imageDesc
                    << "\nmeta.roi[" << n << "].header.timeBor=" << roiHdr->timestampBOR
                    << "\nmeta.roi[" << n << "].header.timeEor=" << roiHdr->timestampEOR;
            }
            imageDesc
                << "\nmeta.roi[" << n << "].header.roi=[" << rgn.s1 << ","
                    << rgn.s2 << "," << rgn.sbin << "," << rgn.p1 << ","
                    << rgn.p2 << "," << rgn.pbin << "]";
            imageDesc
                << "\nmeta.roi[" << n << "].header.flags=" << (uint16_t)roiHdr->flags
                << "\nmeta.roi[" << n << "].header.extMdSize=" << roiHdr->extendedMdSize
                << "\nmeta.roi[" << n << "].dataSize=" << roi.dataSize
                << "\nmeta.roi[" << n << "].extMdSize=" << roi.extMdDataSize;
        }
    }

    return imageDesc.str();
}
