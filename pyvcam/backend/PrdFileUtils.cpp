#include "PrdFileUtils.h"

/* System */
#include <cstring>
#include <limits>

void ClearPrdHeaderStructure(PrdHeader& header)
{
    memset(&header, 0, sizeof(PrdHeader));
    header.signature = PRD_SIGNATURE;
}

size_t GetRawDataSizeInBytes(const PrdHeader& header)
{
    const PrdRegion& region = header.region;
    if (region.sbin == 0 || region.pbin == 0)
        return 0;
    size_t bytes;
    if (header.version >= PRD_VERSION_0_3)
    {
        bytes = header.frameSize;
    }
    else
    {
        const uint16_t width = (region.s2 - region.s1 + 1) / region.sbin;
        const uint16_t height = (region.p2 - region.p1 + 1) / region.pbin;
        bytes = sizeof(uint16_t) * width * height;
    }
    return bytes;
}

size_t GetPrdFileSizeOverheadInBytes(const PrdHeader& header)
{
    return sizeof(PrdHeader) + header.frameCount * header.sizeOfPrdMetaDataStruct;
}

size_t GetPrdFileSizeInBytes(const PrdHeader& header)
{
    const size_t rawDataBytes = GetRawDataSizeInBytes(header);
    if (rawDataBytes == 0)
        return 0;
    return GetPrdFileSizeOverheadInBytes(header) + header.frameCount * rawDataBytes;
}

uint32_t GetFrameCountThatFitsIn(const PrdHeader& header, size_t maxSizeInBytes)
{
    const size_t rawDataBytes = GetRawDataSizeInBytes(header);
    if (rawDataBytes == 0 || maxSizeInBytes <= sizeof(PrdHeader))
        return 0;
    const size_t count = (maxSizeInBytes - sizeof(PrdHeader))
        / (header.sizeOfPrdMetaDataStruct + rawDataBytes);
    if (count > std::numeric_limits<uint32_t>::max())
        return 0;
    return (uint32_t)count;
}

const void* GetExtMetadataAddress(const PrdHeader& header, const void* metadata,
        uint32_t extFlag)
{
    if (!metadata)
        return nullptr;

    // Extended metadata added in PRD_VERSION_0_5
    if (header.version < PRD_VERSION_0_5)
        return nullptr;

    auto prdMeta = static_cast<const PrdMetaData*>(metadata);

    const uint32_t extMetaOffset =
        header.sizeOfPrdMetaDataStruct - prdMeta->extMetaDataSize;
    auto extMeta = static_cast<const uint8_t*>(metadata) + extMetaOffset;

    const PrdTrajectoriesHeader* extMetaTrajectories = nullptr;
    if (header.version >= PRD_VERSION_0_5)
    {
        if (prdMeta->extFlags & PRD_EXT_FLAG_HAS_TRAJECTORIES)
        {
            extMetaTrajectories =
                reinterpret_cast<const PrdTrajectoriesHeader*>(extMeta);
            extMeta += GetTrajectoriesSizeInBytes(extMetaTrajectories);
        }
    }

    //const PrdXyz* extMetaXyz = nullptr;
    //if (header.version >= PRD_VERSION_0_6)
    //{
    //    if (prdMeta->extFlags & PRD_EXT_FLAG_HAS_XYZ)
    //    {
    //        extMetaXyz = reinterpret_cast<const PrdXyz*>(extMeta);
    //        extMeta += GetXyzSizeInBytes(extMetaXyz);
    //    }
    //}

    switch (extFlag)
    {
    case PRD_EXT_FLAG_HAS_TRAJECTORIES:
        return extMetaTrajectories;
    //case PRD_EXT_FLAG_HAS_XYZ:
    //    return extMetaXyz;
    default:
        return nullptr;
    };
}

uint32_t GetTrajectoriesSizeInBytes(const PrdTrajectoriesHeader* trajectoriesHeader)
{
    if (!trajectoriesHeader)
        return 0;

    if (trajectoriesHeader->maxTrajectories == 0
            && trajectoriesHeader->maxTrajectoryPoints == 0)
        return 0;

    const uint32_t onePointSize = sizeof(PrdTrajectoryPoint);
    const uint32_t allPointsSize =
        trajectoriesHeader->maxTrajectoryPoints * onePointSize;

    const uint32_t oneTrajectorySize =
        sizeof(PrdTrajectoryHeader) + allPointsSize;
    const uint32_t allTrajectoriesSize =
        trajectoriesHeader->maxTrajectories * oneTrajectorySize;

    const uint32_t totalTrajectoriesSize =
        sizeof(PrdTrajectoriesHeader) + allTrajectoriesSize;

    return totalTrajectoriesSize;
}

bool ConvertTrajectoriesFromPrd(const PrdTrajectoriesHeader* from,
        pm::Frame::Trajectories& to)
{
    if (!from)
        return false;

    if (from->maxTrajectories < from->trajectoryCount)
        return false;

    if (from->maxTrajectories == 0 && from->maxTrajectoryPoints == 0)
        return true;

    void* dst;
    // The src is non-const just to be able to move the pointer to next data
    auto src = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(from));
    uint32_t size;

    // Add trajectories header
    dst = &to.header;
    size = sizeof(PrdTrajectoriesHeader);
    std::memcpy(dst, src, size);
    src += size;

    for (uint32_t n = 0; n < from->trajectoryCount; ++n)
    {
        pm::Frame::Trajectory trajectory;

        // Add trajectory header
        dst = &trajectory.header;
        size = sizeof(PrdTrajectoryHeader);
        std::memcpy(dst, src, size);
        src += size;

        if (from->maxTrajectoryPoints < trajectory.header.pointCount)
            return false;

        // Resize vector to right size
        trajectory.data.resize(trajectory.header.pointCount);

        // Add valid trajectory points
        dst = trajectory.data.data();
        size = sizeof(PrdTrajectoryPoint) * trajectory.header.pointCount;
        std::memcpy(dst, src, size);
        // Move over all points including unused space up to capacity
        src += sizeof(PrdTrajectoryPoint) * from->maxTrajectoryPoints;

        // Add trajectory to trajectories vector
        to.data.push_back(trajectory);
    }

    return true;
}

bool ConvertTrajectoriesToPrd(const pm::Frame::Trajectories& from,
        PrdTrajectoriesHeader* to)
{
    if (!to)
        return false;

    if (from.header.maxTrajectories < from.header.trajectoryCount)
        return false;

    if (from.data.size() != from.header.trajectoryCount)
        return false;

    if (from.header.maxTrajectories == 0 && from.header.maxTrajectoryPoints == 0)
        return true;

    auto dst = reinterpret_cast<uint8_t*>(to);
    const void* src;
    uint32_t size;

    // Add trajectories header
    src = &from.header;
    size = sizeof(PrdTrajectoriesHeader);
    std::memcpy(dst, src, size);
    dst += size;

    for (uint32_t n = 0; n < from.header.trajectoryCount; ++n)
    {
        const pm::Frame::Trajectory& trajectory = from.data.at(n);

        if (from.header.maxTrajectoryPoints < trajectory.header.pointCount)
            return false;

        if (trajectory.data.size() != trajectory.header.pointCount)
            return false;

        // Add trajectory header
        src = &trajectory.header;
        size = sizeof(PrdTrajectoryHeader);
        std::memcpy(dst, src, size);
        dst += size;

        // Add valid trajectory points
        src = trajectory.data.data();
        size = sizeof(PrdTrajectoryPoint) * trajectory.header.pointCount;
        std::memcpy(dst, src, size);
        // Move over all points including unused space up to capacity
        dst += sizeof(PrdTrajectoryPoint) * from.header.maxTrajectoryPoints;
    }

    return true;
}

std::shared_ptr<pm::Frame> ReconstructFrame(const PrdHeader& header,
        const void* metaData, const void* /*extDynMetaData*/, const void* rawData)
{
    if (!rawData || !metaData)
        return nullptr;

    auto prdMeta = static_cast<const PrdMetaData*>(metaData);

    const size_t rawDataSize = GetRawDataSizeInBytes(header);
    const uint16_t roiCount = prdMeta->roiCount;
    const bool hasMetadata = ((header.flags & PRD_FLAG_HAS_METADATA) != 0);

    pm::Frame::AcqCfg acqCfg(rawDataSize, roiCount, hasMetadata);

    auto framePtr = new(std::nothrow) pm::Frame(acqCfg, true);
    if (!framePtr)
        return nullptr;

    auto frame = std::shared_ptr<pm::Frame>(framePtr);

    frame->SetDataPointer(const_cast<void*>(rawData));
    if (!frame->CopyData())
    {
        return nullptr;
    }

    const uint32_t frameNr = prdMeta->frameNumber;
    uint64_t timestampBOF = prdMeta->bofTime;
    uint64_t timestampEOF = prdMeta->eofTime;
    if (header.version >= PRD_VERSION_0_4)
    {
        timestampBOF |= ((uint64_t)prdMeta->bofTimeHigh << 32);
        timestampEOF |= ((uint64_t)prdMeta->eofTimeHigh << 32);
    }

    pm::Frame::Info info(frameNr, timestampBOF, timestampEOF);

    frame->SetInfo(info);

    auto trajectoriesAddress =
        GetExtMetadataAddress(header, metaData, PRD_EXT_FLAG_HAS_TRAJECTORIES);
    if (trajectoriesAddress)
    {
        auto prdTrajectories =
            static_cast<const PrdTrajectoriesHeader*>(trajectoriesAddress);
        pm::Frame::Trajectories trajectories;

        if (!ConvertTrajectoriesFromPrd(prdTrajectories, trajectories))
            return nullptr;

        frame->SetTrajectories(trajectories);
    }


    return frame;
}
