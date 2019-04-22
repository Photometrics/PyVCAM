#pragma once
#ifndef _PRD_FILE_FORMAT_H
#define _PRD_FILE_FORMAT_H

/* System */
#include <stdint.h>

/* TODO: Current implementation supports PRD format on architectures
         with little endian only. Remove that limitation. */

/// Identifies PRD file format in PrdHeader.signature (null-terminated string "PRD").
#define PRD_SIGNATURE   ((uint32_t)0x00445250)

/** PRD file versions.
    Higher version must have higher number assigned.
    @{ */
/// PRD version 0.1
#define PRD_VERSION_0_1 ((uint16_t)0x0001)
/// PRD version 0.2
#define PRD_VERSION_0_2 ((uint16_t)0x0002)
/// PRD version 0.3
#define PRD_VERSION_0_3 ((uint16_t)0x0003)
/// PRD version 0.4
#define PRD_VERSION_0_4 ((uint16_t)0x0004)
/// PRD version 0.5
#define PRD_VERSION_0_5 ((uint16_t)0x0005)
/** @} */

/** PRD exposure resolutions.
    @{ */
/// Exposure resolution in microseconds.
#define PRD_EXP_RES_US  ((uint32_t)1)
/// Exposure resolution in milliseconds.
#define PRD_EXP_RES_MS  ((uint32_t)1000)
/// Exposure resolution in seconds.
#define PRD_EXP_RES_S   ((uint32_t)1000000)
/** @} */

/** PRD frame flags (bits).
    @{ */
/// Raw frame data contains also PVCAM metadata, not only pixel data.
#define PRD_FLAG_HAS_METADATA       ((uint8_t)0x01)
/// A file contains multiple frames which size might not be the same.
/** Extended dynamic metadata size is *not* included in
    PrdHeader.sizeOfPrdMetaDataStruct value.
    Because of that fact such files cannot be open with older tools that
    don't understand PRD_VERSION_0_5 format or newer. */
#define PRD_FLAG_FRAME_SIZE_VARY    ((uint8_t)0x02)
/** @} */

/** PRD extended metadata flags (bits).
    @{ */
/// Frame has particle trajectories.
#define PRD_EXT_FLAG_HAS_TRAJECTORIES   ((uint32_t)0x00000001)
/** @} */

// Deny compiler to align these structures
#pragma pack(push)
#pragma pack(1)

/// Structure describing the area and binning factor used for acquisition.
/// PrdRegion type is compatible with PVCAM rgn_type type.
struct PrdRegion // 12 bytes
{
    /// First serial/horizontal pixel.
    uint16_t s1; // 2 bytes
    /// Last serial/horizontal pixel.
    /// Must be equal or greater than s1.
    uint16_t s2; // 2 bytes
    /// Serial/horizontal binning.
    /// Must not be zero.
    uint16_t sbin; // 2 bytes
    /// First parallel/vertical pixel.
    uint16_t p1; // 2 bytes
    /// Last parallel/vertical pixel.
    /// Must be equal or greater than p1.
    uint16_t p2; // 2 bytes
    /// Parallel/vertical binning.
    /// Must not be zero.
    uint16_t pbin; // 2 bytes
};
typedef struct PrdRegion PrdRegion;

/// PRD (Photometrics Raw Data) file format.
/** Numbers in all structures are stored in little endian.
    PRD file consists of:
    - PrdHeader structure
    - PrdHeader.frameCount times repeated:
      - Metadata (PrdHeader.sizeOfPrdMetaDataStruct bytes)
        - PrdMetaData structure
        - Extended metadata (constant size)
          (only if PrdMetaData.extMetaDataSize is non-zero)
          - If has flag PRD_EXT_FLAG_HAS_TRAJECTORIES:
            - PrdTrajectoriesHeader structure
            - PrdTrajectoriesHeader.maxTrajectories times repeated:
              - PrdTrajectoryHeader structure
              - PrdTrajectoriesHeader.maxTrajectoryPoints times repeated:
                - PrdTrajectoryPoint structure
      - Optional extended dynamic metadata (variable size)
        (only if PrdHeader.flags has PRD_FLAG_FRAME_SIZE_VARY set)
        - Not used yet
      - RAW frame data, always 2 bytes per pixel
        (with metadata if PrdHeader.flags has PRD_FLAG_HAS_METADATA set) */
// The size of PrdHeader should stay 48 bytes and never change!
struct PrdHeader // 48 bytes
{
    /** Members introduced in PRD_VERSION_0_1
        @{ */

    /// Has to contain PRD_SIGNATURE value.
    // Was 'char type[4]' which is compatible
    uint32_t signature; // 4 bytes

    /// Contains one of PRD_VERSION_* macro values.
    uint16_t version; // 2 bytes
    /// Raw data bit depth taken from camera (but every pixel is stored in 16 bits).
    uint16_t bitDepth; // 2 bytes

    /// Usually 1, but for stack might be greater than 1.
    uint32_t frameCount; // 4 bytes

    /// Used chip region in pixels and binning.
    /** This region can have a bit different meaning depending on file version
        and metadata.
        - Frame without PVCAM metadata - Only one ROI can be setup for
            acquisition which is directly stored in here.
        - Frame with PVCAM metadata (supported since PRD_VERSION_0_3)
            - Multi-ROI frame - The frame consists of multiple static regions
                specified by user. In this case #region specifies calculated
                implied ROI containing all given regions.
            - Frame with centroids - The frame consists of multiple small and
                dynamically generated regions (by camera). With centroids only
                one ROI can be setup for acquisition by user which is directly
                stored in here. Please note that the implied ROI as stored in
                PVCAM metadata structures is not the same and is within that
                #region.
        Anyway, it always defines the dimensions of final image reconstructed
        from raw data.
        Calculate image width  (uint16_t) from region as: (s2 - s1 + 1) / sbin.
        Calculate image height (uint16_t) from region as: (p2 - p1 + 1) / pbin.
    */
    PrdRegion region; // 12 bytes

    /// Size of PrdMetaData structure used while saving.
    /** Since PRD_VERSION_0_5 it contains size of PrdMetaData together with
        size of extended metadata. */
    uint32_t sizeOfPrdMetaDataStruct; // 4 bytes

    /// Exposure resolution.
    /// Is one of PRD_EXP_RES_* macro values.
    uint32_t exposureResolution; // 4 bytes

    /** @} */ /* PRD_VERSION_0_1 */

    /** Members introduced in PRD_VERSION_0_3
        @{ */

    /// Color mask (correcponds to PL_COLOR_MODES).
    uint8_t colorMask; // 1 byte
    /// Contains ORed combination of PRD_FLAG_* macro values.
    uint8_t flags; // 1 byte

    /// Size of frame raw data in bytes.
    /** For frame without metadata the size can be calculated from #region.
        Size of the frame with metadata depends on number of ROIs/centroids,
        extended metadata size, etc. */
    uint32_t frameSize; // 4 bytes

    /** @} */ /* PRD_VERSION_0_3 */

    /// Reserved space used only for structure alignment at the moment.
    uint8_t _reserved[10]; // 10 bytes
};
typedef struct PrdHeader PrdHeader;

/// Detailed information about captured frame.
struct PrdMetaData // 48 bytes
{
    /** Members introduced in PRD_VERSION_0_1
        @{ */

    /// Frame index, should be unique and first is 1.
    uint32_t frameNumber; // 4 bytes
    /// Readout time in microseconds (does not include exposure time).
    uint32_t readoutTime; // 4 bytes

    /// Exposure time in micro-, milli- or seconds, depends on exposureResolution.
    uint32_t exposureTime; // 4 bytes

    /** @} */ /* PRD_VERSION_0_1 */

    /** Members introduced in PRD_VERSION_0_2
        @{ */

    /// BOF time in microseconds (taken from acquisition start).
    uint32_t bofTime; // 4 bytes
    /// EOF time in microseconds (taken from acquisition start).
    uint32_t eofTime; // 4 bytes

    /** @} */ /* PRD_VERSION_0_2 */

    /** Members introduced in PRD_VERSION_0_3
        @{ */

    /// ROI count (1 for frames without PRD_FLAG_HAS_METADATA flag).
    uint16_t roiCount; // 2 bytes

    /** @} */ /* PRD_VERSION_0_3 */

    /** Members introduced in PRD_VERSION_0_4
        @{ */

    /// Upper 4 byte of BOF time in microseconds (taken from acquisition start).
    uint32_t bofTimeHigh; // 4 bytes
    /// Upper 4 byte of EOF time in microseconds (taken from acquisition start).
    uint32_t eofTimeHigh; // 4 bytes

    /** @} */ /* PRD_VERSION_0_4 */

    /** Members introduced in PRD_VERSION_0_5
        @{ */

    /// Contains ORed combination of PRD_EXT_FLAG_* macro values.
    /** If the flag is not set, related extended metadata is missing. */
    uint32_t extFlags; // 4 bytes

    /// The size of extended metadata (same for all frames).
    /** Extended metadata follows the PrdMetaData structure in same order as are
        PRD_EXT_FLAG_* flags declared (based on numeric flag value, from
        lowest to highest). */
    /** Extended metadata size is included in PrdHeader.sizeOfPrdMetaDataStruct
        value. The offset of first extended metadata byte per frame is:
        PrdHeader.sizeOfPrdMetaDataStruct - extMetaDataSize. */
    uint32_t extMetaDataSize; // 4 bytes

    /// The size of extended dynamic metadata (might be different for each frame).
    /** Can be non-zero only if PRD_FLAG_FRAME_SIZE_VARY is set in
        PrdHeader.flags. */
    /** Extended dynamic metadata follows extended metadata in same order as are
        PRD_EXT_FLAG_DYN_* flags declared (based on numeric flag value, from
        lowest to highest). */
    /** Extended dynamic metadata size is *not* included in
        PrdHeader.sizeOfPrdMetaDataStruct value. The offset of first extended
        dynamic metadata byte per frame is PrdHeader.sizeOfPrdMetaDataStruct.
        Because of that fact such files cannot be open with older tools that
        don't understand PRD_VERSION_0_5 format or newer. */
    /** It is not used at the moment, the value should be 0. */
    uint32_t extDynMetaDataSize; // 4 bytes

    /** @} */ /* PRD_VERSION_0_5 */

    /// Reserved space used only for structure alignment at the moment.
    uint8_t _reserved[6]; // 6 bytes

    // Extended metadata starts here.
};
typedef struct PrdMetaData PrdMetaData;

/** Members introduced in PRD_VERSION_0_5
    @{ */

/// Trajectories for one frame.
struct PrdTrajectoriesHeader // 12 bytes
{
    /// Max. number of supported trajectories in each frame.
    /** The real size of all trajectories data in frame is:
            maxTrajectories
                * (sizeof(PrdTrajectoryHeader)
                    + maxTrajectoryPoints * sizeof(PrdTrajectoryPoint)).
        Number of valid trajectories is given by trajectoryCount.
        For stack file (a file with multiple frames in it) the capacity has to
        be same for all frames, so all frames have the same size. */
    uint32_t maxTrajectories; // 4 bytes

    /// Max. number of supported points in each trajectory.
    /** The real size of all points data in trajectory is:
            maxTrajectoryPoints * sizeof(PrdTrajectoryPoint).
        Number of valid points is given by PrdTrajectoryHeader.pointCount.
        For stack file (a file with multiple frames in it) the capacity has to
        be same for all frames, so all frames have the same size. */
    uint32_t maxTrajectoryPoints; // 4 bytes

    /// Number of trajectories.
    uint32_t trajectoryCount; // 4 bytes
};
typedef struct PrdTrajectoriesHeader PrdTrajectoriesHeader;

/// Trajectory for one particle.
struct PrdTrajectoryHeader // 14 bytes
{
    /// Related ROI number for current frame.
    /** This is used to find the part of the image (a ROI) with particle this
        trajectory is related to, i.e. mapping within current frame. */
    uint16_t roiNr; // 2 bytes

    /// Particle ID of trajectory.
    /** This is used to find the same particles on other frames, i.e. mapping
        across the frames. */
    uint32_t particleId; // 4 bytes

    /// Number of frames the particle has been detected in.
    /** If the particle disappeared for one or a few frames and the linking
        algorithm still recognizes and marks it with same @p particleID,
        the @p lifetime is not increased for frames where it was missing. */
    uint32_t lifetime; // 4 bytes

    /// Number of points in trajectory.
    uint32_t pointCount; // 4 bytes
};
typedef struct PrdTrajectoryHeader PrdTrajectoryHeader;

/// Point the trajectory is build of.
struct PrdTrajectoryPoint // 5 bytes
{
    /// Zero means invalid, any other value means point is valid.
    uint8_t isValid; // 1 byte
    /// Offset in sensor coordinates without binning applied.
    uint16_t x; // 2 bytes
    /// Offset in sensor coordinates without binning applied.
    uint16_t y; // 2 bytes
};
typedef struct PrdTrajectoryPoint PrdTrajectoryPoint;

/** @} */ /* PRD_VERSION_0_5 */

// Restore default alignment
#pragma pack(pop)

#endif /* _PRD_FILE_FORMAT_H */
