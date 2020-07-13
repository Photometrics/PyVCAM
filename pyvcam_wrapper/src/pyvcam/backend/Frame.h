#pragma once
#ifndef PM_FRAME_H
#define PM_FRAME_H

/* Local */
#include "PrdFileFormat.h"

/* System */
#include <atomic>
#include <cstdint>
#include <cstring> // memset
#include <map>
#include <memory> // std::shared_ptr
#include <utility> // std::pair
#include <vector>

/* PVCAM */
#include <master.h>
#include <pvcam.h>

namespace pm {

class Frame
{
public:
    struct Trajectory
    {
        Trajectory() : header(), data() { memset(&header, 0, sizeof(header)); }
        PrdTrajectoryHeader header;
        std::vector<PrdTrajectoryPoint> data;
    };

    struct Trajectories
    {
        Trajectories() : header(), data() { memset(&header, 0, sizeof(header)); }
        PrdTrajectoriesHeader header;
        std::vector<Trajectory> data;
    };

public:
    class AcqCfg
    {
    public:
        AcqCfg();
        AcqCfg(size_t frameBytes, uint16_t roiCount, bool hasMetadata);
        AcqCfg(const AcqCfg& other);
        AcqCfg& operator=(const AcqCfg& other);
        bool operator==(const AcqCfg& other) const;
        bool operator!=(const AcqCfg& other) const;

        size_t GetFrameBytes() const;
        void SetFrameBytes(size_t frameBytes);

        uint16_t GetRoiCount() const;
        void SetRoiCount(uint16_t roiCount);

        bool HasMetadata() const;
        void SetMetadata(bool hasMetadata);

    private:
        size_t m_frameBytes;
        uint16_t m_roiCount;
        bool m_hasMetadata;
    };

public:
    class Info
    {
    public:
        Info();
        Info(uint32_t frameNr, uint64_t timestampBOF, uint64_t m_timestampEOF);
        Info(const Info& other);
        Info& operator=(const Info& other);
        bool operator==(const Info& other) const;
        bool operator!=(const Info& other) const;

        uint32_t GetFrameNr() const;
        uint64_t GetTimestampBOF() const;
        uint64_t GetTimestampEOF() const;
        uint32_t GetReadoutTime() const;

    private:
        uint32_t m_frameNr;
        uint64_t m_timestampBOF;
        uint64_t m_timestampEOF;
        uint32_t m_readoutTime;
    };

public:
    explicit Frame(const AcqCfg& acqCfg, bool deepCopy);
    ~Frame();

    Frame() = delete;
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    const Frame::AcqCfg& GetAcqCfg() const;
    bool UsesDeepCopy() const;

    /* Stores only pointer to data without copying it.
       To copy the data itself call CopyData method. */
    void SetDataPointer(void* data);
    /* Invalidates the frame and makes a deep copy with data pointer stored by
       SetDataPointers.
       This is usually done by another thread and it is *not* responsibility
       of this class to watch that stored data pointer still points to valid
       data.
       If frame was created with deepCopy set to false, the deep copy is not
       needed but you still should call this function.
       The metadata if available has to be decoded again.
       Upon successful data copy, either shallow or deep, the frame is set to
       valid. This is the only method that can set frame to be valid. */
    bool CopyData();

    const void* GetData() const;

    bool IsValid() const;
    /* Invalidates frame, clears frame info, trajectories, metadata, etc. */
    void Invalidate();
    /* Should be used in very rare cases where you know what you're doing */
    void OverrideValidity(bool isValid);

    const Frame::Info& GetInfo() const;
    void SetInfo(const Frame::Info& frameInfo);

    /* Returns previously set list of <X, Y> coordinates in sensor area for each
       particle detected in frame data. For each particle first item in array
       are coordinates of given particle on this frame, next item is location of
       the same particle on previous frame, and so on up to max. number of
       frames configured by user.*/
    const Frame::Trajectories& GetTrajectories() const;
    void SetTrajectories(const Frame::Trajectories& trajectories);

    /* Decodes frame metadata if AcqCfg::HasMetadata is set. The method returns
       immediately if metadata has already been decoded.
       Method returns without error if frame has no metadata. */
    bool DecodeMetadata();
    /* Returns decoded metadata or null if frame has no metadata.
       DecodeMetadata should be always called before this method to ensure the
       returned value is valid. */
    const md_frame* GetMetadata() const;
    const std::map<uint16_t, md_ext_item_collection>& GetExtMetadata() const;

    // TODO: Implement
    //bool Recompose();
    //const void* GetRecomposedData() const;

    /* Copies all from other to this frame. Utilizes private Copy method.
       If deepCopy is false, this method only stores frame info and calls
       SetDataPointer method. CopyData method has to be called separately.
       Unlike the Clone method, this one does not change the value returned by
       UsesDeepCopy method. */
    bool Copy(const Frame& other, bool deepCopy = true);
    /* Returns new copy of this frame. Utilizes private Copy method. */
    std::shared_ptr<Frame> Clone(bool deepCopy = true) const;

private:
    /* Uses SetDataPointer and CopyData methods to make the copy. It also copies
       frame info and trajectories. Metadata if available has to be decoded by
       calling DecodeMetadata method as we cannot safely do a deep copy. */
    bool Copy(const Frame& from, Frame& to, bool deepCopy = true) const;

private:
    const Frame::AcqCfg m_acqCfg;
    const bool m_deepCopy;

    void* m_data;
    void* m_dataSrc;

    std::atomic<bool> m_isValid;

    Frame::Info m_info;
    Frame::Info m_shallowInfo;
    Frame::Trajectories m_trajectories;

    std::atomic<bool> m_needsDecoding;
    md_frame* m_metadata;
    // Returns map<roiNr, md_ext_item_collection>
    std::map<uint16_t, md_ext_item_collection> m_extMetadata;
};

} // namespace pm

#endif /* PM_FRAME_H */
