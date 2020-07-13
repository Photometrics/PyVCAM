#pragma once
#ifndef _PRD_FILE_UTILS_H
#define _PRD_FILE_UTILS_H

/* Local */
#include "Frame.h"
#include "PrdFileFormat.h"

/* System */
#include <cstdlib>

/// Function initializes PrdHeader structure with zeroes and sets its type member.
void ClearPrdHeaderStructure(PrdHeader& header);

/// Calculates RAW data size.
/** It requires only following header members: region. */
size_t GetRawDataSizeInBytes(const PrdHeader& header);

/// Calculates PRD file data overhead from its header.
/** It requires only following header members: frameCount and sizeOfPrdMetaDataStruct.
    Returned size does not include possible extended dynamic metadata. */
size_t GetPrdFileSizeOverheadInBytes(const PrdHeader& header);

/// Calculates size of whole PRD file from its header.
/** It requires only following header members: region, frameCount and sizeOfPrdMetaDataStruct.
    Returned size does not include possible extended dynamic metadata. */
size_t GetPrdFileSizeInBytes(const PrdHeader& header);

/// Calculates max. number of frames in PRD file that fits into given limit.
/** It requires only following header members: region and sizeOfPrdMetaDataStruct.
    Returned value is restricted to uint32_t by PrdHeader.frameCount type.
    It returns 0 if in given size fits more frames than can be stored in uint32_t. */
uint32_t GetFrameCountThatFitsIn(const PrdHeader& header, size_t maxSizeInBytes);

/// Returns beginning of extended metadata block for given flag.
/** The given flag has to be one of PRD_EXT_FLAG_* values, not the combined
    value as stored in PrdMetaData structure. If more flags are combined, or if
    there is no extended metadata, function returns null. */
const void* GetExtMetadataAddress(const PrdHeader& header, const void* metadata,
        uint32_t extFlag);

/// Calculates number of bytes required to store given trajectories.
/** The size includes also all headers as described in PRD file format.
    The size is calculated for the given capacity, not from current number of
    trajectories and points in each trajectory. */
uint32_t GetTrajectoriesSizeInBytes(const PrdTrajectoriesHeader* trajectoriesHeader);

/// Converts trajectories from raw data as stored in PRD file to C++ containers.
/** If #GetTrajectoriesSizeInBytes(@a from) returns zero, the @a to argument is
    not touched. Thus it should be zeroed or default-initialized before call. */
bool ConvertTrajectoriesFromPrd(const PrdTrajectoriesHeader* from,
        pm::Frame::Trajectories& to);

/// Converts trajectories from C++ containers to raw data as stored in PRD file.
/** If the capacity of trajectories and points in each trajectory are zero, the
    @a to argument is not touched. Thus it should be zeroed before call. */
bool ConvertTrajectoriesToPrd(const pm::Frame::Trajectories& from,
        PrdTrajectoriesHeader* to);

/// Reconstructs whole frame from file.
/** If everything goes well, new Frame instance is allocated, filled with data,
    trajectories, etc. On error a null is returned. */
std::shared_ptr<pm::Frame> ReconstructFrame(const PrdHeader& header,
        const void* metaData, const void* extDynMetaData, const void* rawData);

#endif /* _PRD_FILE_UTILS_H */
