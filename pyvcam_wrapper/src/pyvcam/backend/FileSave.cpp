#include "FileSave.h"

/* Local */
#include "PrdFileUtils.h"

/* System */
#include <cstdlib> // std::malloc
#include <cstring> // std::memcpy

pm::FileSave::FileSave(const std::string& fileName, const PrdHeader& header)
    : File(fileName),
    m_header(header),
    m_width((header.region.sbin == 0)
        ? 0
        : (header.region.s2 - header.region.s1 + 1) / header.region.sbin),
    m_height((header.region.pbin == 0)
        ? 0
        : (header.region.p2 - header.region.p1 + 1) / header.region.pbin),
    m_rawDataBytes(GetRawDataSizeInBytes(header)),
    m_framePrdMetaData(nullptr),
    m_framePrdExtDynMetaData(nullptr),
    m_framePrdExtDynMetaDataBytes(0),
    m_frameOrigSizeOfPrdMetaDataStruct(0),
    m_framePrdMetaDataExtFlags(0),
    m_trajectoriesBytes(0)
{
}

pm::FileSave::~FileSave()
{
}

const PrdHeader& pm::FileSave::GetHeader() const
{
    return m_header;
}

void pm::FileSave::Close()
{
    m_trajectoriesBytes = 0;

    std::free(m_framePrdMetaData);
    std::free(m_framePrdExtDynMetaData);
}

bool pm::FileSave::WriteFrame(const void* metaData, const void* extDynMetaData,
        const void* rawData)
{
    if (!IsOpen())
        return false;

    if (!metaData || !rawData)
        return false;

    if (m_width == 0 || m_height == 0 || m_rawDataBytes == 0
            || m_header.sizeOfPrdMetaDataStruct == 0)
        return false;

    if (m_header.version >= PRD_VERSION_0_5)
    {
        if (m_header.flags & PRD_FLAG_FRAME_SIZE_VARY)
        {
            auto prdMetaData = static_cast<const PrdMetaData*>(metaData);
            if (prdMetaData->extDynMetaDataSize > 0 && !extDynMetaData)
                return false;
        }
    }

    return true;
}

bool pm::FileSave::WriteFrame(const Frame& frame, uint32_t expTime)
{
    if (!IsOpen())
        return false;

    if (m_width == 0 || m_height == 0 || m_rawDataBytes == 0
            || m_header.sizeOfPrdMetaDataStruct == 0)
        return false;

    // Get the right metadata size including ext. metadata
    if (m_frameOrigSizeOfPrdMetaDataStruct == 0)
    {
        // Go this way only once
        m_frameOrigSizeOfPrdMetaDataStruct = m_header.sizeOfPrdMetaDataStruct;

        m_trajectoriesBytes =
            GetTrajectoriesSizeInBytes(&frame.GetTrajectories().header);

        const uint32_t extMetadataBytes = GetExtMetaDataSizeInBytes(frame);

        m_header.sizeOfPrdMetaDataStruct += extMetadataBytes;

        m_framePrdMetaData = std::malloc(m_header.sizeOfPrdMetaDataStruct);
        if (!m_framePrdMetaData)
            return false;

        if (m_trajectoriesBytes > 0)
        {
            m_framePrdMetaDataExtFlags |= PRD_EXT_FLAG_HAS_TRAJECTORIES;
        }
    }

    // Do not use std::memset, spec. says behavior might be undefined for C structs
    memset(m_framePrdMetaData, 0, m_header.sizeOfPrdMetaDataStruct);

    // Set basic metadata

    auto metaData = static_cast<PrdMetaData*>(m_framePrdMetaData);

    const Frame::Info& fi = frame.GetInfo();
    const uint64_t bof = fi.GetTimestampBOF() * 100;
    const uint64_t eof = fi.GetTimestampEOF() * 100;

    if (m_header.version >= PRD_VERSION_0_1)
    {
        metaData->frameNumber = fi.GetFrameNr();
        metaData->readoutTime = fi.GetReadoutTime() * 100;
        metaData->exposureTime = expTime;
    }
    if (m_header.version >= PRD_VERSION_0_2)
    {
        metaData->bofTime = (uint32_t)(bof & 0xFFFFFFFF);
        metaData->eofTime = (uint32_t)(eof & 0xFFFFFFFF);
    }
    if (m_header.version >= PRD_VERSION_0_3)
    {
        metaData->roiCount = (uint16_t)frame.GetAcqCfg().GetRoiCount();
    }
    if (m_header.version >= PRD_VERSION_0_4)
    {
        metaData->bofTimeHigh = (uint32_t)((bof >> 32) & 0xFFFFFFFF);
        metaData->eofTimeHigh = (uint32_t)((eof >> 32) & 0xFFFFFFFF);
    }
    if (m_header.version >= PRD_VERSION_0_5)
    {
        metaData->extFlags = m_framePrdMetaDataExtFlags;
        metaData->extMetaDataSize =
            m_header.sizeOfPrdMetaDataStruct - m_frameOrigSizeOfPrdMetaDataStruct;
        metaData->extDynMetaDataSize = 0; // Updated in UpdateFrameExtDynMetaData

        if (!UpdateFrameExtMetaData(frame))
            return false;

        if (!UpdateFrameExtDynMetaData(frame))
            return false;
    }

    return true;
}

bool pm::FileSave::UpdateFrameExtMetaData(const Frame& frame)
{
    auto dest = static_cast<uint8_t*>(m_framePrdMetaData);
    dest += m_frameOrigSizeOfPrdMetaDataStruct;

    if (m_header.version >= PRD_VERSION_0_5)
    {
        const Frame::Trajectories& from = frame.GetTrajectories();
        auto to = reinterpret_cast<PrdTrajectoriesHeader*>(dest);

        const uint32_t size = GetTrajectoriesSizeInBytes(&from.header);
        if (size != m_trajectoriesBytes)
            return false;

        if (m_trajectoriesBytes > 0)
        {
            if (!ConvertTrajectoriesToPrd(from, to))
                return false;
            dest += m_trajectoriesBytes;
        }
    }

    return true;
}

bool pm::FileSave::UpdateFrameExtDynMetaData(const Frame& /*frame*/)
{
    if (!(m_header.flags & PRD_FLAG_FRAME_SIZE_VARY))
        return true;

    // No extended dynamic metadata so far.
    // In future it should be stored in given frame.
    const void* extDynMetaData = nullptr;
    uint32_t extDynMetaDataBytes = 0;

    if (!extDynMetaData || extDynMetaDataBytes == 0)
        return true;

    // Resize internal buffer to new size if differs
    if (m_framePrdExtDynMetaDataBytes != extDynMetaDataBytes)
    {
        void* newMem = std::realloc(m_framePrdExtDynMetaData, extDynMetaDataBytes);
        if (!newMem)
            return false;

        m_framePrdExtDynMetaData = newMem;
        m_framePrdExtDynMetaDataBytes = extDynMetaDataBytes;

        auto metaData = static_cast<PrdMetaData*>(m_framePrdMetaData);
        metaData->extDynMetaDataSize = extDynMetaDataBytes;
    }

    // Store the data
    std::memcpy(m_framePrdExtDynMetaData, extDynMetaData, extDynMetaDataBytes);

    return true;
}

uint32_t pm::FileSave::GetExtMetaDataSizeInBytes(const pm::Frame& /*frame*/)
{
    if (m_header.version < PRD_VERSION_0_5)
        return 0;

    uint32_t size = 0;

    if (m_header.version >= PRD_VERSION_0_5)
    {
        size += m_trajectoriesBytes;
    }

    return size;
}
