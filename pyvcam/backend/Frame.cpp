#include "Frame.h"

/* PVCAM */
#include <master.h>
#include <pvcam.h>

/* System */
#include <algorithm> // std::for_each, std::min
#include <cstring>
#include <iomanip> // std::setfill, std::setw
#include <new>
#include <sstream>

/* Local */
#include "Log.h"

// pm::Frame::AcqCfg

pm::Frame::AcqCfg::AcqCfg()
    : m_frameBytes(0),
    m_roiCount(0),
    m_hasMetadata(false)
{
}

pm::Frame::AcqCfg::AcqCfg(size_t frameBytes, uint16_t roiCount, bool hasMetadata)
    : m_frameBytes(frameBytes),
    m_roiCount(roiCount),
    m_hasMetadata(hasMetadata)
{
}

pm::Frame::AcqCfg::AcqCfg(const Frame::AcqCfg& other)
    : m_frameBytes(other.m_frameBytes),
    m_roiCount(other.m_roiCount),
    m_hasMetadata(other.m_hasMetadata)
{
}

pm::Frame::AcqCfg& pm::Frame::AcqCfg::operator=(const Frame::AcqCfg& other)
{
    if (&other != this)
    {
        m_frameBytes = other.m_frameBytes;
        m_roiCount = other.m_roiCount;
        m_hasMetadata = other.m_hasMetadata;
    }
    return *this;
}

bool pm::Frame::AcqCfg::operator==(const Frame::AcqCfg& other) const
{
    return m_frameBytes == other.m_frameBytes
        && m_roiCount == other.m_roiCount
        && m_hasMetadata == other.m_hasMetadata;
}

bool pm::Frame::AcqCfg::operator!=(const Frame::AcqCfg& other) const
{
    return !(*this == other);
}

size_t pm::Frame::AcqCfg::GetFrameBytes() const
{
    return m_frameBytes;
}

void pm::Frame::AcqCfg::SetFrameBytes(size_t frameBytes)
{
    m_frameBytes = frameBytes;
}

uint16_t pm::Frame::AcqCfg::GetRoiCount() const
{
    return m_roiCount;
}

void pm::Frame::AcqCfg::SetRoiCount(uint16_t roiCount)
{
    m_roiCount = roiCount;
}

bool pm::Frame::AcqCfg::HasMetadata() const
{
    return m_hasMetadata;
}

void pm::Frame::AcqCfg::SetMetadata(bool hasMetadata)
{
    m_hasMetadata = hasMetadata;
}

// pm::Frame::Info

pm::Frame::Info::Info()
    : m_frameNr(0),
    m_timestampBOF(0),
    m_timestampEOF(0),
    m_readoutTime(0)
{
}

pm::Frame::Info::Info(uint32_t frameNr, uint64_t timestampBOF, uint64_t m_timestampEOF)
    : m_frameNr(frameNr),
    m_timestampBOF(timestampBOF),
    m_timestampEOF(m_timestampEOF),
    m_readoutTime((uint32_t)(m_timestampEOF - m_timestampBOF))
{
}

pm::Frame::Info::Info(const Frame::Info& other)
    : m_frameNr(other.m_frameNr),
    m_timestampBOF(other.m_timestampBOF),
    m_timestampEOF(other.m_timestampEOF),
    m_readoutTime(other.m_readoutTime)
{
}

pm::Frame::Info& pm::Frame::Info::operator=(const Frame::Info& other)
{
    if (&other != this)
    {
        m_frameNr = other.m_frameNr;
        m_timestampBOF = other.m_timestampBOF;
        m_timestampEOF = other.m_timestampEOF;
        m_readoutTime = other.m_readoutTime;
    }
    return *this;
}

bool pm::Frame::Info::operator==(const Frame::Info& other) const
{
    return m_frameNr == other.m_frameNr
        && m_timestampBOF == other.m_timestampBOF
        && m_timestampEOF == other.m_timestampEOF;
}

bool pm::Frame::Info::operator!=(const Frame::Info& other) const
{
    return !(*this == other);
}

uint32_t pm::Frame::Info::GetFrameNr() const
{
    return m_frameNr;
}

uint64_t pm::Frame::Info::GetTimestampBOF() const
{
    return m_timestampBOF;
}

uint64_t pm::Frame::Info::GetTimestampEOF() const
{
    return m_timestampEOF;
}

uint32_t pm::Frame::Info::GetReadoutTime() const
{
    return m_readoutTime;
}

// pm::Frame

pm::Frame::Frame(const AcqCfg& acqCfg, bool deepCopy)
    : m_acqCfg(acqCfg),
    m_deepCopy(deepCopy),
    m_data(nullptr),
    m_dataSrc(nullptr),
    m_isValid(false),
    m_info(),
    m_shallowInfo(),
    m_trajectories(),
    m_needsDecoding(false),
    m_metadata(nullptr),
    m_extMetadata()
{
    if (m_deepCopy && m_acqCfg.GetFrameBytes() > 0)
    {
        m_data = (void*)new(std::nothrow) uint8_t[m_acqCfg.GetFrameBytes()];
        if (!m_data)
        {
            Log::LogE("Failed to allocate frame data");
        }
    }

    if (m_acqCfg.HasMetadata())
    {
        if (PV_OK != pl_md_create_frame_struct_cont(&m_metadata,
                    m_acqCfg.GetRoiCount()))
        {
            Log::LogE("Failed to allocate frame metadata structure");
        }
    }
}

pm::Frame::~Frame()
{
    if (m_deepCopy)
    {
        // m_data must not be deleted on shallow copy
        delete [] (uint8_t*)m_data;
    }

    if (m_metadata)
    {
        // Ignore errors
        pl_md_release_frame_struct(m_metadata);
    }
}

const pm::Frame::AcqCfg& pm::Frame::GetAcqCfg() const
{
    return m_acqCfg;
}

bool pm::Frame::UsesDeepCopy() const
{
    return m_deepCopy;
}

void pm::Frame::SetDataPointer(void* data)
{
    m_dataSrc = data;
}

bool pm::Frame::CopyData()
{
    Invalidate();

    if (!m_dataSrc)
    {
        Log::LogE("Invalid source data pointer");
        return false;
    }

    if (m_acqCfg.HasMetadata() && !m_metadata)
    {
        Log::LogE("Invalid metadata pointer");
        return false;
    }

    if (m_deepCopy)
    {
        if (!m_data)
        {
            Log::LogE("Invalid data pointer");
            return false;
        }
        memcpy(m_data, m_dataSrc, m_acqCfg.GetFrameBytes());
    }
    else
    {
        m_data = m_dataSrc;
    }

    const auto emptyInfo = Frame::Info();
    if (m_shallowInfo != emptyInfo)
    {
        m_info = m_shallowInfo;
        m_shallowInfo = emptyInfo;
    }

    m_isValid = true;

    return true;
}

const void* pm::Frame::GetData() const
{
    return m_data;
}

bool pm::Frame::IsValid() const
{
    return m_isValid;
}

void pm::Frame::Invalidate()
{
    m_isValid = false;

    m_info = Frame::Info();
    m_trajectories = Frame::Trajectories();

    m_needsDecoding = m_acqCfg.HasMetadata();
    if (m_metadata)
    {
        // Invalidate metadata
        m_metadata->roiCount = 0;
    }
    m_extMetadata.clear();
}

void pm::Frame::OverrideValidity(bool isValid)
{
    m_isValid = isValid;
}

const pm::Frame::Info& pm::Frame::GetInfo() const
{
    return m_info;
}

void pm::Frame::SetInfo(const Frame::Info& info)
{
    m_info = info;
}

const pm::Frame::Trajectories& pm::Frame::GetTrajectories() const
{
    return m_trajectories;
}

void pm::Frame::SetTrajectories(const Frame::Trajectories& trajectories)
{
    m_trajectories = trajectories;
}

bool pm::Frame::DecodeMetadata()
{
    if (!m_needsDecoding)
        return true;

    if (!m_isValid)
    {
        Log::LogE("Invalid frame");
        return false;
    }

    const uns32 frameBytes = (uns32)m_acqCfg.GetFrameBytes();

    if (PV_OK != pl_md_frame_decode(m_metadata, m_data, frameBytes))
    {
        const int16 errId = pl_error_code();
        char errMsg[ERROR_MSG_LEN] = "<unknown>";
        pl_error_message(errId, errMsg);

        const uint8_t* dumpData = (uint8_t*)m_data;
        const uint32_t dumpDataBytes = std::min<uint32_t>(frameBytes, 32u);
        std::ostringstream dumpStream;
        dumpStream << std::hex << std::uppercase << std::setfill('0');
        std::for_each(dumpData, dumpData + dumpDataBytes, [&dumpStream](uint8_t byte) {
            dumpStream << ' ' << std::setw(2) << (unsigned int)byte;
        });

        Log::LogE("Unable to decode frame %u (%s), addr: 0x%p, data: %s",
                m_info.GetFrameNr(), errMsg, m_data, dumpStream.str().c_str());

        Invalidate();
        return false;
    }

    for (uint16_t n = 0; n < m_metadata->roiCount; ++n)
    {
        const md_frame_roi& mdRoi = m_metadata->roiArray[n];

        if (mdRoi.extMdDataSize == 0)
            continue;

        md_ext_item_collection* collection = &m_extMetadata[mdRoi.header->roiNr];

        // Extract extended metadata from ROI
        if (PV_OK != pl_md_read_extended(collection, mdRoi.extMdData,
                    mdRoi.extMdDataSize))
        {
            const int16 errId = pl_error_code();
            char errMsg[ERROR_MSG_LEN] = "<unknown>";
            pl_error_message(errId, errMsg);

            Log::LogE("Failed to read ext. metadata for frame nr. %u (%s)",
                    m_info.GetFrameNr(), errMsg);

            Invalidate();
            return false;
        }
    }

    m_needsDecoding = false;

    return true;
}

const md_frame* pm::Frame::GetMetadata() const
{
    return m_metadata;
}

const std::map<uint16_t, md_ext_item_collection>& pm::Frame::GetExtMetadata() const
{
    return m_extMetadata;
}

bool pm::Frame::Copy(const Frame& other, bool deepCopy)
{
    return Copy(other, *this, deepCopy);
}

std::shared_ptr<pm::Frame> pm::Frame::Clone(bool deepCopy) const
{
    std::shared_ptr<pm::Frame> frame;

    try
    {
        frame = std::make_shared<Frame>(m_acqCfg, deepCopy);
    }
    catch (...)
    {
        return nullptr;
    }

    if (!Copy(*this, *frame, true))
    {
        // Probably never happens
        return nullptr;
    }

    return frame;
}

bool pm::Frame::Copy(const Frame& from, Frame& to, bool deepCopy)
{
    if (from.m_acqCfg != to.m_acqCfg)
    {
        to.Invalidate();
        Log::LogE("Failed to copy frame due to configuration mismatch");
        return false;
    }

    to.SetDataPointer(from.m_data);

    if (deepCopy)
    {
        if (!to.CopyData())
            return false;

        to.SetInfo(from.m_info);
        to.SetTrajectories(from.m_trajectories);

        to.m_shallowInfo = Frame::Info();
    }
    else
    {
        to.SetInfo(from.m_info);
        to.m_shallowInfo = from.m_info;
    }

    return true;
}
