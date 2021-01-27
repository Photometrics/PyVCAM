#ifdef _MSC_VER
// Suppress warning C4996 on generate_n functions
#define _SCL_SECURE_NO_WARNINGS
// Suppress warning C4996 on strncpy functions
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "FakeCamera.h"

/* System */
#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <random>
#include <sstream>
#include <thread>

/* Local */
#include "Frame.h"
#include "Log.h"
#include "Utils.h"

#define MAX_GEN_FRAME_COUNT 10

// Camera properties
// TODO: Read from some config file
const char* const g_cameraName = "FakeCamera";
const uns16 g_sensorWidth = 640;
const uns16 g_sensorHeight = 360;
const uns16 g_sensorBitDepth = 14;
const char* const g_port0Name = "FakePort0";
const char* const g_gain1Name = "FakeGain1";
const char* const g_chipName = "FakeChipName";
const char* const g_serialNumber = "FakeSerial";
const uns16 g_roiCountMax = 1; // TODO: Change later when supported metadata
const uns16 g_centroidCountMax = 500;
const uns16 g_centroidRadiusMax = 50;

const std::map<int32, md_ext_item_info> g_pl_ext_md_map = {
    //{ PL_MD_EXT_TAG_PARTICLE_ID, { (PL_MD_EXT_TAGS)PL_MD_EXT_TAG_PARTICLE_ID, TYPE_UNS32, 4, "Particle ID" } },
    { PL_MD_EXT_TAG_PARTICLE_M0, { (PL_MD_EXT_TAGS)PL_MD_EXT_TAG_PARTICLE_M0, TYPE_UNS32, 4, "Particle M0" } },
    { PL_MD_EXT_TAG_PARTICLE_M2, { (PL_MD_EXT_TAGS)PL_MD_EXT_TAG_PARTICLE_M2, TYPE_UNS32, 4, "Particle M2" } },
};

bool pm::FakeCamera::s_isInitialized = false;

pm::FakeCamera::FakeCamera(unsigned int targetFps)
    : m_targetFps(targetFps),
    m_readoutTimeMs(1000 / targetFps),
    m_frameRoiExtMdSize(0),
    m_particleCoordinates(),
    m_particleMoments(),
    m_callbackHandler(nullptr),
    m_callbackContext(nullptr),
    m_startStopTimer(),
    m_frameGenBuffer(nullptr),
    m_frameGenBufferPos(0),
    m_frameGenFrameIndex(0),
    m_frameGenFrameInfo(),
    m_frameGenStopFlag(true),
    m_frameGenThread(nullptr),
    m_frameGenMutex()
{
    memset(&m_frameGenFrameInfo, 0, sizeof(m_frameGenFrameInfo));

    for (auto it : g_pl_ext_md_map)
    {
        m_frameRoiExtMdSize += sizeof(uns8); // For tag ID
        m_frameRoiExtMdSize += it.second.size;
    }
}

pm::FakeCamera::~FakeCamera()
{
    StopExp();
    Close();
}

unsigned int pm::FakeCamera::GetTargetFps() const
{
    return m_targetFps;
}

bool pm::FakeCamera::Initialize()
{
    if (s_isInitialized)
        return true;

    Log::LogI("Using fake camera set to %u FPS\n", m_targetFps);

    s_isInitialized = true;
    return true;
}

bool pm::FakeCamera::Uninitialize()
{
    if (!s_isInitialized)
        return true;

    s_isInitialized = false;
    return true;
}

bool pm::FakeCamera::GetCameraCount(int16& count) const
{
    count = 1;
    return true;
}

bool pm::FakeCamera::GetName(int16 index, std::string& name) const
{
    name.clear();

    if (index != 0)
    {
        Log::LogE("Failed to get name for camera at index %d", index);
        return false;
    }

    name = g_cameraName;
    return true;
}

std::string pm::FakeCamera::GetErrorMessage() const
{
    // Method not needed in FakeCamera, see other error logs
    return "N/A";
}

bool pm::FakeCamera::Open(const std::string& name)
{
    if (m_isOpen)
        return true;

    if (name != g_cameraName)
    {
        Log::LogE("Failure opening camera '%s'", name.c_str());
        return false;
    }

    m_hCam = 0;

    return Camera::Open(name);
}

bool pm::FakeCamera::Close()
{
    if (!m_isOpen)
        return true;

    DeleteBuffers();

    m_hCam = -1;

    return Camera::Close();
}

// Returns the desired size (number of frames) of the circular (or sequence)
// frame buffer
uns32 pm::FakeCamera::GetDesiredFrameBufferSizeInFrames(const SettingsReader& settings)
{
    // If the settings specify a buffer size, just use it outright
    uns32 frame_count = m_settings.GetBufferFrameCount();
    if (frame_count > 0)
        return frame_count;

    switch (m_settings.GetAcqMode())
    {
        case AcqMode::SnapSequence:
            // buffer size = number of frames to acquire
            frame_count = m_settings.GetAcqFrameCount();
            break;

        case AcqMode::SnapCircBuffer:
        case AcqMode::LiveCircBuffer:
            frame_count = 50;
            break;

        case AcqMode::SnapTimeLapse:
        case AcqMode::LiveTimeLapse:
            frame_count = 1;
            break;
    }

    return frame_count;
}

bool pm::FakeCamera::SetupExp(const SettingsReader& settings)
{
    if (!Camera::SetupExp(settings))
        return false;

    if (m_settings.GetRegions().size() != 1)
    {
        Log::LogE("Unsupported number of regions (%llu)",
                (unsigned long long)m_settings.GetRegions().size());
        return false;
    }

    const uns32 frameCount = GetDesiredFrameBufferSizeInFrames(settings);
    const uns32 frameBytes = CalculateFrameBytes();

    if (!AllocateBuffers(frameCount, frameBytes))
        return false;

    return true;
}

bool pm::FakeCamera::StartExp(CallbackEx3Fn callbackHandler, void* callbackContext)
{
    if (!callbackHandler || !callbackContext)
        return false;

    m_callbackHandler = callbackHandler;
    m_callbackContext = callbackContext;

    m_frameGenBufferPos = 0;
    m_frameGenFrameIndex = 0;

    m_startStopTimer.Reset();

    /* Start the frame generation thread, which is tasked with creating frames
       of the specified type and artifically calling the proper callback routine */
    m_frameGenStopFlag = false;
    m_frameGenThread =
        new(std::nothrow) std::thread(&FakeCamera::FrameGeneratorLoop, this);
    if (!m_frameGenThread)
    {
        Log::LogE("Failed to start the acquisition");
        return false;
    }

    m_isImaging = true;

    return true;
}

bool pm::FakeCamera::StopExp()
{
    if (m_isImaging)
    {
        if (m_frameGenThread)
        {
            // Tell frame generation thread we're done
            m_frameGenStopFlag = true;
            m_frameGenThread->join();
            delete m_frameGenThread;
            m_frameGenThread = nullptr;
        }

        m_isImaging = false;

        m_callbackHandler = nullptr;
        m_callbackContext = nullptr;
    }

    return true;
}

pm::Camera::AcqStatus pm::FakeCamera::GetAcqStatus() const
{
    return (m_isImaging)
        ? Camera::AcqStatus::Active
        : Camera::AcqStatus::Inactive;
}

bool pm::FakeCamera::SetParam(uns32 id, void* param)
{
    if (!param)
        return false;

    // Every parameter has to be explicitly handled
    switch (id) {

    case PARAM_READOUT_PORT:
        // No need to store value
        return true;

    case PARAM_SPDTAB_INDEX:
        // No need to store value
        return true;

    case PARAM_GAIN_INDEX:
        // No need to store value
        return true;

    case PARAM_CLEAR_MODE:
        // TODO: Store value when needed
        return true;

    case PARAM_CLEAR_CYCLES:
        // TODO: Store value when needed
        return true;

    case PARAM_PMODE:
        // No need to store value
        return true;

    case PARAM_EXP_RES:
        // No need to store value
        return true;

    case PARAM_EXP_RES_INDEX:
        // No need to store value
        return true;

    case PARAM_METADATA_ENABLED:
        // No need to store value
        return true;

    case PARAM_CENTROIDS_ENABLED:
        // No need to store value
        return true;

    case PARAM_CENTROIDS_MODE:
        // No need to store value
        return true;

    case PARAM_CENTROIDS_COUNT:
        // No need to store value
        return true;

    case PARAM_CENTROIDS_RADIUS:
        // No need to store value
        return true;

    case PARAM_CENTROIDS_BG_COUNT:
        // No need to store value
        return true;

    case PARAM_CENTROIDS_THRESHOLD:
        // No need to store value
        return true;

    default:
        break;
    }

    Log::LogE("FakeCamera::SetParam(id=%u, <value>) NOT IMPLEMENTED", id);
    return false;
}

bool pm::FakeCamera::GetParam(uns32 id, int16 attr, void* param) const
{
    if (!param)
        return false;

    // Every parameter has to be explicitly handled
    switch (id) {

    case PARAM_CAM_INTERFACE_TYPE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(int32*)param = PL_CAM_IFC_TYPE_VIRTUAL;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CAM_INTERFACE_MODE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(int32*)param = PL_CAM_IFC_MODE_IMAGING;
            return true;
        default:
            break;
        }
        break;

    case PARAM_READOUT_PORT:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_SPDTAB_INDEX:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_COUNT:
            *(uns32*)param = 1;
            return true;
        default:
            break;
        }
        break;

    case PARAM_GAIN_INDEX:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_MAX:
            *(int16*)param = 1;
            return true;
        default:
            break;
        }
        break;

    case PARAM_GAIN_NAME:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            strncpy((char*)param, g_gain1Name, MAX_GAIN_NAME_LEN);
            return true;
        default:
            break;
        }
        break;

    case PARAM_GAIN_MULT_FACTOR:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = FALSE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_SER_SIZE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(uns16*)param = g_sensorWidth;
            return true;
        default:
            break;
        }
        break;

    case PARAM_PAR_SIZE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(uns16*)param = g_sensorHeight;
            return true;
        default:
            break;
        }
        break;

    case PARAM_BIT_DEPTH:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(uns16*)param = g_sensorBitDepth;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CHIP_NAME:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            strncpy((char*)param, g_chipName, CCD_NAME_LEN);
            return true;
        default:
            break;
        }
        break;

    case PARAM_HEAD_SER_NUM_ALPHA:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            strncpy((char*)param, g_serialNumber, MAX_ALPHA_SER_NUM_LEN);
            return true;
        default:
            break;
        }
        break;

    case PARAM_CIRC_BUFFER:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CLEAR_MODE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
        case ATTR_DEFAULT:
            *(int32*)param = CLEAR_PRE_EXPOSURE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CLEAR_CYCLES:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(uns16*)param = 2;
            return true;
        default:
            break;
        }
        break;

    case PARAM_EXP_RES:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(int32*)param = EXP_RES_ONE_MILLISEC;
            return true;
        default:
            break;
        }
        break;

    case PARAM_EXP_RES_INDEX:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(int32*)param = EXP_RES_ONE_MILLISEC;
            return true;
        default:
            break;
        }
        break;

    case PARAM_ROI_COUNT:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_MAX:
            *(uns16*)param = g_roiCountMax;
            return true;
        default:
            break;
        }
        break;

    case PARAM_METADATA_ENABLED:
        switch (attr) {
        case ATTR_AVAIL:
        case ATTR_CURRENT:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_COLOR_MODE:
        switch (attr) {
        case ATTR_AVAIL:
            // Parameter is not available for mono camera
            *(rs_bool*)param = FALSE;
            return true;
        //case ATTR_CURRENT:
        //    *(int32*)param = COLOR_NONE;
        //    return true;
        default:
            break;
        }
        break;

    case PARAM_CENTROIDS_ENABLED:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CENTROIDS_MODE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CENTROIDS_COUNT:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_MAX:
            *(uns16*)param = g_centroidCountMax;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CENTROIDS_RADIUS:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_MAX:
            *(uns16*)param = g_centroidRadiusMax;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CENTROIDS_BG_COUNT:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_DEFAULT:
            *(int32*)param = 0;
            return true;
        default:
            break;
        }
        break;

    case PARAM_CENTROIDS_THRESHOLD:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_DEFAULT:
            *(uns32*)param = 160; // Q8.4 ~ 10.0
            return true;
        default:
            break;
        }
        break;

    case PARAM_TRIGTAB_SIGNAL:
        switch (attr) {
        case ATTR_AVAIL:
            // TODO: Change later when supported triggering table
            *(rs_bool*)param = FALSE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_LAST_MUXED_SIGNAL:
        switch (attr) {
        case ATTR_AVAIL:
            // TODO: Change later when supported triggering table
            *(rs_bool*)param = FALSE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_EXPOSURE_MODE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_EXPOSE_OUT_MODE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_PMODE:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_PIX_TIME:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        case ATTR_CURRENT:
            *(uns16*)param = 1;
            return true;
        default:
            break;
        }
        break;

    case PARAM_BINNING_SER:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    case PARAM_BINNING_PAR:
        switch (attr) {
        case ATTR_AVAIL:
            *(rs_bool*)param = TRUE;
            return true;
        default:
            break;
        }
        break;

    default:
        break;
    }

    Log::LogE("FakeCamera::GetParam(id=%u, attr=%d, <value>) NOT IMPLEMENTED",
            id, attr);
    return false;
}

bool pm::FakeCamera::GetEnumParam(uns32 id, std::vector<EnumItem>& items) const
{
    items.clear();

    EnumItem ei;

    // Every parameter has to be explicitly handled
    switch (id) {

    case PARAM_CAM_INTERFACE_TYPE:
        ei.value = PL_CAM_IFC_TYPE_VIRTUAL;
        ei.desc = "FakeVirtual";
        items.push_back(ei);
        return true;

    case PARAM_CAM_INTERFACE_MODE:
        ei.value = PL_CAM_IFC_MODE_IMAGING;
        ei.desc = "FakeImaging";
        items.push_back(ei);
        return true;

    case PARAM_READOUT_PORT:
        ei.value = 0;
        ei.desc = g_port0Name;
        items.push_back(ei);
        return true;

    case PARAM_CLEAR_MODE:
        ei.value = CLEAR_PRE_EXPOSURE;
        ei.desc = "FakePreExposure";
        items.push_back(ei);
        return true;

    case PARAM_EXP_RES:
        ei.value = EXP_RES_ONE_MILLISEC;
        ei.desc = "FakeMilliSec";
        items.push_back(ei);
        return true;

    case PARAM_EXPOSURE_MODE:
        ei.value = EXT_TRIG_INTERNAL;
        ei.desc = "FakeExtInternal";
        items.push_back(ei);
        return true;

    case PARAM_EXPOSE_OUT_MODE:
        ei.value = EXPOSE_OUT_FIRST_ROW;
        ei.desc = "FakeFirstRow";
        items.push_back(ei);
        return true;

    case PARAM_PMODE:
        ei.value = PMODE_NORMAL;
        ei.desc = "FakeNormal";
        items.push_back(ei);
        return true;

    case PARAM_BINNING_SER:
        ei.value = 1;
        ei.desc = "1x1";
        items.push_back(ei);
        return true;

    case PARAM_BINNING_PAR:
        ei.value = 1;
        ei.desc = "1x1";
        items.push_back(ei);
        return true;

    case PARAM_CENTROIDS_MODE:
        ei.value = PL_CENTROIDS_MODE_LOCATE;
        ei.desc = "FakeLocate";
        items.push_back(ei);
        ei.value = PL_CENTROIDS_MODE_TRACK;
        ei.desc = "FakeTrack";
        items.push_back(ei);
        return true;

    case PARAM_CENTROIDS_BG_COUNT:
        ei.value = 0;
        ei.desc = "10";
        items.push_back(ei);
        ei.value = 1;
        ei.desc = "50";
        items.push_back(ei);
        return true;

    default:
        break;
    }

    Log::LogE("FakeCamera::GetEnumParam(id=%u, <values>) NOT IMPLEMENTED", id);
    return false;
}

bool pm::FakeCamera::GetLatestFrame(Frame& frame) const
{
    std::unique_lock<std::mutex> lock(m_frameGenMutex);

    const size_t index = m_frameGenBufferPos;

    m_frames[index]->Invalidate();
    frame.Invalidate();

    const uint32_t oldFrameNr = m_frames[index]->GetInfo().GetFrameNr();
    const Frame::Info fi(
            (uint32_t)m_frameGenFrameInfo.FrameNr,
            (uint64_t)m_frameGenFrameInfo.TimeStampBOF,
            (uint64_t)m_frameGenFrameInfo.TimeStamp);
    m_frames[index]->SetInfo(fi);
    UpdateFrameIndexMap(oldFrameNr, index);

    return frame.Copy(*m_frames[index], false);
}

bool pm::FakeCamera::AllocateBuffers(uns32 frameCount, uns32 frameBytes)
{
    if (!Camera::AllocateBuffers(frameCount, frameBytes))
        return false;

    const size_t bufferBytes = MAX_GEN_FRAME_COUNT * m_frameAcqCfg.GetFrameBytes();
    m_frameGenBuffer =
        (void*)(new(std::nothrow) uns8[bufferBytes]);
    if (!m_frameGenBuffer)
    {
        Log::LogE("Failure allocating fake image buffer with %u bytes", bufferBytes);
        return false;
    }

    // TODO: Check errors
    GenerateFrameData();

    return true;
}

void pm::FakeCamera::DeleteBuffers()
{
    Camera::DeleteBuffers();

    delete [] (uns8*)m_frameGenBuffer;
    m_frameGenBuffer = nullptr;
}

uns32 pm::FakeCamera::CalculateFrameBytes() const
{
    // TODO: Adjust the size based on current settings, now hard-coded for particles

    // TODO: Currently limited to one ROI only
    const rgn_type rgn = m_settings.GetRegions().at(0);

    const uns32 rgnBytes = sizeof(uns16)
        * ((rgn.s2 + 1 - rgn.s1) / rgn.sbin)
        * ((rgn.p2 + 1 - rgn.p1) / rgn.pbin);

    // Background ROI size (full ROI image, includes invalid ext. metadata)
    const uns32 bgRoiSize =
        sizeof(md_frame_roi_header) + m_frameRoiExtMdSize + rgnBytes;

    // Patch ROI size (header only, no image data)
    const uns32 patchDataSize = 0; // Two bytes per pixel
    const uns32 patchTotalSize =
        sizeof(md_frame_roi_header) + m_frameRoiExtMdSize + patchDataSize;

    const uns16 centroidsCount = m_settings.GetCentroidsCount();

    const uns32 frameBytes =
        sizeof(md_frame_header) + bgRoiSize + centroidsCount * patchTotalSize;

    return frameBytes;
}

void pm::FakeCamera::InjectParticles(uns16* pBuffer, uns16 particleIntensity)
{
    // A particle is a bright pixel inside the given ROI
    for (const std::pair<uns16, uns16>& coordinate : m_particleCoordinates)
    {
        const int pCX = coordinate.first;
        const int pCY = coordinate.second;
        // Now we need to edit the pixel(s) in the flat buffer
        const size_t indexC = g_sensorWidth * pCY + pCX;
        const size_t indexL = g_sensorWidth * pCY + pCX - 1;
        const size_t indexR = g_sensorWidth * pCY + pCX + 1;
        const size_t indexT = g_sensorWidth * (pCY - 1) + pCX;
        const size_t indexB = g_sensorWidth * (pCY + 1) + pCX;
        pBuffer[indexT] = particleIntensity;
        pBuffer[indexL] = particleIntensity;
        pBuffer[indexC] = particleIntensity;
        pBuffer[indexR] = particleIntensity;
        pBuffer[indexB] = particleIntensity;
    }
}

md_frame_header pm::FakeCamera::GenerateFrameHeader(uns32 frameIndex)
{
    const uns16 centroidsCount = m_settings.GetCentroidsCount();

    md_frame_header header;

    header.signature = PL_MD_FRAME_SIGNATURE;
    header.version = 1;

    header.frameNr = frameIndex + 1; // Frame numbers are 1-based
    header.roiCount = static_cast<uns16>(centroidsCount + 1);

    header.exposureTime = m_settings.GetExposure();
    header.exposureTimeResNs = 1000000; // 1ms (or 1000000ns)

    header.timestampBOF =
        frameIndex * header.exposureTime * header.exposureTimeResNs;
    header.timestampEOF =
        header.timestampBOF + m_readoutTimeMs * header.exposureTimeResNs;
    header.timestampResNs = 1000000; // 1ms

    header.roiTimestampResNs = 1000000; // 1ms

    header.bitDepth = g_sensorBitDepth;
    header.colorMask = COLOR_NONE;
    header.flags = 0x0;
    header.extendedMdSize = 0;

    return header;
}

md_frame_roi_header pm::FakeCamera::GenerateParticleHeader(uns16 roiIndex,
        uns16 x, uns16 y)
{
    const uns16 radius = m_settings.GetCentroidsRadius();
    const uns16 s1 = x - radius;
    const uns16 s2 = x + radius;
    const uns16 sbin = m_settings.GetBinningSerial();
    const uns16 p1 = y - radius;
    const uns16 p2 = y + radius;
    const uns16 pbin = m_settings.GetBinningParallel();
    const rgn_type rgn = { s1, s2, sbin, p1, p2, pbin };

    const uns16 centroidsCount = m_settings.GetCentroidsCount();

    md_frame_roi_header roiHdr;

    // Fill-in all ROI header data
    roiHdr.roiNr = roiIndex + 2; // ROI numbers are 1-based, the first is the background frame
    roiHdr.timestampBOR =
        ((1000000 * m_readoutTimeMs) / centroidsCount) * roiIndex;
    roiHdr.timestampEOR =
        roiHdr.timestampBOR + ((1000000 * m_readoutTimeMs) / centroidsCount);
    roiHdr.roi = rgn;
    roiHdr.extendedMdSize = 0; // Filled in later
    roiHdr.flags = PL_MD_ROI_FLAG_HEADER_ONLY;

    return roiHdr;
}

void pm::FakeCamera::GenerateParticles()
{
    std::random_device random;

    const uns16 radius = m_settings.GetCentroidsRadius();
    const uns16 centroidsCount = m_settings.GetCentroidsCount();

    m_particleCoordinates.clear();
    for (int i = 0; i < centroidsCount; i++)
    {
        const uns16 x = radius + (random() % (g_sensorWidth - 2 * radius));
        const uns16 y = radius + (random() % (g_sensorHeight - 2 * radius));
        m_particleCoordinates.emplace_back(x, y);

        // Fixed-point float in format Q22.0
        const uns32 m0 = (random() % 0x3FFFFF);
        // Fixed-point float in format Q3.19
        const uns32 m2 = (random() % 0x3FFFFF);
        m_particleMoments.emplace_back(m0, m2);
    }
}

void pm::FakeCamera::MoveParticles()
{
    std::random_device random;

    const uns16 radius = m_settings.GetCentroidsRadius();

    // Generates new random position for particle
    for (size_t i = 0; i < m_particleCoordinates.size(); i++)
    {
        const uns16 oldX = m_particleCoordinates[i].first;
        const uns16 oldY = m_particleCoordinates[i].second;
        uns16 newX = 0;
        uns16 newY = 0;

        // Generates new coordinates till they fit the sensor
        do
        {
            const uns16 step = random() % (m_settings.GetTrackMaxDistance() * 3 / 4);
            const int randomAngle = random() % 360;
            const double radian = randomAngle * 3.14159265358979323846 / 180.0;

            newX = static_cast<uns16>(oldX + step * cos(radian));
            newY = static_cast<uns16>(oldY + step * sin(radian));
        }
        while (newX < 0 + radius || newX >= g_sensorWidth - radius
            || newY < 0 + radius || newY >= g_sensorHeight - radius);

        m_particleCoordinates[i].first = newX;
        m_particleCoordinates[i].second = newY;
    }
}

void pm::FakeCamera::GenerateFrameData()
{
    std::random_device random;

    GenerateParticles();

    const uns16 bgPixValue = (uns16)(((uns32)1 << g_sensorBitDepth) * 1 / 4);
    const uns16 fgPixValue = (uns16)(((uns32)1 << g_sensorBitDepth) * 3 / 4);

    // TODO: Take ROI and binning from settings
    const uns16 s1 = 0;
    const uns16 s2 = g_sensorWidth - 1;
    const uns16 sbin = m_settings.GetBinningSerial();
    const uns16 p1 = 0;
    const uns16 p2 = g_sensorHeight - 1;
    const uns16 pbin = m_settings.GetBinningParallel();
    const rgn_type rgn = { s1, s2, sbin, p1, p2, pbin };

    /* Generate the data as specified, this data will be the same for each frame
       due to significant computation time needed */
    const size_t totalPixels = g_sensorWidth * g_sensorHeight;

    const uns16 centroidsCount = m_settings.GetCentroidsCount();

    for (uns16 i = 0; i < MAX_GEN_FRAME_COUNT; i++)
    {
        uns8* bufferPosition = &((uns8*)m_frameGenBuffer)[i * m_frameAcqCfg.GetFrameBytes()];

        // 1. Add Frame header
        const md_frame_header hdr = GenerateFrameHeader(i);
        *(md_frame_header*)bufferPosition = hdr;
        bufferPosition += sizeof(md_frame_header);

        // 2. Add ROI header for full frame (first ROI)
        md_frame_roi_header fullFrameRoiHdr;
        fullFrameRoiHdr.roiNr = 1; // ROI numbers are 1-based, the first is the background frame
        fullFrameRoiHdr.timestampBOR =
            ((1000000 * m_readoutTimeMs) / centroidsCount) * i;
        fullFrameRoiHdr.timestampEOR = fullFrameRoiHdr.timestampBOR
            + ((1000000 * m_readoutTimeMs) / centroidsCount);
        fullFrameRoiHdr.roi = rgn;
        fullFrameRoiHdr.extendedMdSize = m_frameRoiExtMdSize;
        fullFrameRoiHdr.flags = 0;
        *(md_frame_roi_header*)bufferPosition = fullFrameRoiHdr;
        bufferPosition += sizeof(md_frame_roi_header);
        if (g_pl_ext_md_map.count(PL_MD_EXT_TAG_PARTICLE_ID) > 0)
        {
            *bufferPosition = PL_MD_EXT_TAG_PARTICLE_ID;
            bufferPosition += 1;
            *(uns32*)bufferPosition = 0; // Value doesn't matter
            bufferPosition += g_pl_ext_md_map.at(PL_MD_EXT_TAG_PARTICLE_ID).size;
        }
        if (g_pl_ext_md_map.count(PL_MD_EXT_TAG_PARTICLE_M0) > 0)
        {
            *bufferPosition = PL_MD_EXT_TAG_PARTICLE_M0;
            bufferPosition += 1;
            *(uns32*)bufferPosition = 0; // Value doesn't matter
            bufferPosition += g_pl_ext_md_map.at(PL_MD_EXT_TAG_PARTICLE_M0).size;
        }
        if (g_pl_ext_md_map.count(PL_MD_EXT_TAG_PARTICLE_M2) > 0)
        {
            *bufferPosition = PL_MD_EXT_TAG_PARTICLE_M2;
            bufferPosition += 1;
            *(uns32*)bufferPosition = 0; // Value doesn't matter
            bufferPosition += g_pl_ext_md_map.at(PL_MD_EXT_TAG_PARTICLE_M2).size;
        }

        // 3. Add Image data
        std::generate_n((uns16*)bufferPosition, totalPixels, [&random, bgPixValue]() -> uns16 {
            return random() % bgPixValue;
        });
        InjectParticles((uns16*)bufferPosition, fgPixValue);
        bufferPosition += totalPixels * 2;

        // 4. Add Particle headers
        for (uns16 j = 0; j < centroidsCount; j++)
        {
            md_frame_roi_header particleHeader = GenerateParticleHeader(j,
                    m_particleCoordinates[j].first, m_particleCoordinates[j].second);
            particleHeader.extendedMdSize = m_frameRoiExtMdSize;

            // Our data pointer is at the beginning of a ROI header
            *(md_frame_roi_header*)bufferPosition = particleHeader;
            bufferPosition += sizeof(md_frame_roi_header);

            // Fill in the extended ROI metadata
            if (g_pl_ext_md_map.count(PL_MD_EXT_TAG_PARTICLE_ID) > 0)
            {
                *bufferPosition = PL_MD_EXT_TAG_PARTICLE_ID;
                bufferPosition += 1;
                *(uns32*)bufferPosition = (uns32)j;
                bufferPosition += g_pl_ext_md_map.at(PL_MD_EXT_TAG_PARTICLE_ID).size;
            }
            if (g_pl_ext_md_map.count(PL_MD_EXT_TAG_PARTICLE_M0) > 0)
            {
                *bufferPosition = PL_MD_EXT_TAG_PARTICLE_M0;
                bufferPosition += 1;
                *(uns32*)bufferPosition = m_particleMoments[j].first;
                bufferPosition += g_pl_ext_md_map.at(PL_MD_EXT_TAG_PARTICLE_M0).size;
            }
            if (g_pl_ext_md_map.count(PL_MD_EXT_TAG_PARTICLE_M2) > 0)
            {
                *bufferPosition = PL_MD_EXT_TAG_PARTICLE_M2;
                bufferPosition += 1;
                *(uns32*)bufferPosition = m_particleMoments[j].second;
                bufferPosition += g_pl_ext_md_map.at(PL_MD_EXT_TAG_PARTICLE_M2).size;
            }
        }

        MoveParticles();
    }
}

void pm::FakeCamera::FrameGeneratorLoop()
{
    /* Run in a loop giving one frame at a time to the callback
       routine that is normally called by PVCAM. */

    const AcqMode acqMode = m_settings.GetAcqMode();
    const size_t bufferFrameCount = GetMaxBufferredFrames();
    const unsigned int timeLapseDelay = m_settings.GetTimeLapseDelay();

    const double microsecPerFrame = 1000000.0 / m_targetFps
        + ((acqMode == AcqMode::SnapTimeLapse || acqMode == AcqMode::LiveTimeLapse)
            ? 1000.0 * timeLapseDelay
            : 0);

    // Sleep will be called only for times equal to or greater than this value
    const long long sleepThreshold = 500;

    while (!m_frameGenStopFlag)
    {
        const double microsecNow = m_startStopTimer.Microseconds();
        const double microsecDelay = microsecNow - (microsecPerFrame * m_frameGenFrameIndex);
        const long long sleepTime = (long long)(microsecPerFrame - microsecDelay);

        //Log::LogD("microsecNow: %f, microsecDelay: %f, sleepTime: %lld%s\n",
        //        microsecNow, microsecDelay, sleepTime,
        //        ((sleepTime > sleepThreshold) ? " - SLEEPING" : ""));

        /* Wait the proper time, so that the frame rate matches that specified
           in the command line. */
        if (sleepTime > sleepThreshold)
            std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));

        // Set frame info values
        {
            std::unique_lock<std::mutex> lock(m_frameGenMutex);
            m_frameGenFrameInfo.FrameNr =
                (int32)(m_frameGenFrameIndex & std::numeric_limits<int32>::max()) + 1;
            // Frame's camera handle is 0
            // Keep frame's time stamps and readout time empty for now
        }

        // Set frame data
        const size_t frameBytes = m_frameAcqCfg.GetFrameBytes();
        void* dst = (void*)&((uns8*)m_buffer)[m_frameGenBufferPos * frameBytes];
        const void* src = (void*)&((uns8*)m_frameGenBuffer)[
            (m_frameGenFrameIndex % MAX_GEN_FRAME_COUNT) * frameBytes];
        memcpy(dst, src, frameBytes);

        // TODO: Update metadata headers if any

        // Call the callback
        // TODO: Run this function in separate thread to emulate PVCAM behavior
        m_callbackHandler(&m_frameGenFrameInfo, m_callbackContext);

        // Update counters for frame
        m_frameGenFrameIndex++;
        m_frameGenBufferPos = m_frameGenFrameIndex % bufferFrameCount;
    }
}
