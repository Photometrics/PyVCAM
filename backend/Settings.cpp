#include "Settings.h"

/* Local */
#include "Log.h"
#include "Utils.h"

using SettingsProto = bool(pm::Settings::*)(const std::string&);

// pm::Settings::ReadOnlyWriter

pm::Settings::ReadOnlyWriter::ReadOnlyWriter(Settings& settings)
    : m_settings(settings)
{
}

bool pm::Settings::ReadOnlyWriter::SetEmGainCapable(bool value)
{
    m_settings.m_emGainCapable = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetEmGainMax(uns16 value)
{
    m_settings.m_emGainMax = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetBitDepth(uns16 value)
{
    m_settings.m_bitDepth = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetWidth(uns16 value)
{
    m_settings.m_width = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetHeight(uns16 value)
{
    m_settings.m_height = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetCircBufferCapable(bool value)
{
    m_settings.m_circBufferCapable = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetMetadataCapable(bool value)
{
    m_settings.m_metadataCapable = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetColorMask(int32 value)
{
    m_settings.m_colorMask = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetRegionCountMax(uns16 value)
{
    m_settings.m_regionCountMax = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetCentroidsCapable(bool value)
{
    m_settings.m_centroidsCapable = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetCentroidsModeCapable(bool value)
{
    m_settings.m_centroidsModeCapable = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetCentroidsCountMax(uns16 value)
{
    m_settings.m_centroidsCountMax = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetCentroidsRadiusMax(uns16 value)
{
    m_settings.m_centroidsRadiusMax = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetCentroidsBgCountCapable(bool value)
{
    m_settings.m_centroidsBgCountCapable = value;
    return true;
}

bool pm::Settings::ReadOnlyWriter::SetCentroidsThresholdCapable(bool value)
{
    m_settings.m_centroidsThresholdCapable = value;
    return true;
}

// pm::Settings

pm::Settings::Settings()
    : SettingsReader(),
    m_readOnlyWriter(*this)
{
}

pm::Settings::~Settings()
{
}

pm::Settings::ReadOnlyWriter& pm::Settings::GetReadOnlyWriter()
{
    return m_readOnlyWriter;
}

bool pm::Settings::SetCamIndex(int16 value)
{
    m_camIndex = value;
    return true;
}

bool pm::Settings::SetPortIndex(int32 value)
{
    m_portIndex = value;
    return true;
}

bool pm::Settings::SetSpeedIndex(int16 value)
{
    m_speedIndex = value;
    return true;
}

bool pm::Settings::SetGainIndex(int16 value)
{
    m_gainIndex = value;
    return true;
}

bool pm::Settings::SetEmGain(uns16 value)
{
    m_emGain = value;
    return true;
}

bool pm::Settings::SetClrCycles(uns16 value)
{
    m_clrCycles = value;
    return true;
}

bool pm::Settings::SetClrMode(int32 value)
{
    m_clrMode = value;
    return true;
}

bool pm::Settings::SetPMode(int32 value)
{
    m_pMode = value;
    return true;
}

bool pm::Settings::SetTrigMode(int32 value)
{
    m_trigMode = value;
    return true;
}

bool pm::Settings::SetExpOutMode(int32 value)
{
    m_expOutMode = value;
    return true;
}

bool pm::Settings::SetMetadataEnabled(bool value)
{
    m_metadataEnabled = value;
    return true;
}

bool pm::Settings::SetTrigTabSignal(int32 value)
{
    m_trigTabSignal = value;
    return true;
}

bool pm::Settings::SetLastMuxedSignal(uns8 value)
{
    m_lastMuxedSignal = value;
    return true;
}

bool pm::Settings::SetExposureResolution(int32 value)
{
    switch (value)
    {
    case EXP_RES_ONE_MICROSEC:
    case EXP_RES_ONE_MILLISEC:
    case EXP_RES_ONE_SEC:
        m_expTimeRes = value;
        return true;
    default:
        return false;
    }
}

bool pm::Settings::SetAcqFrameCount(uns32 value)
{
    m_acqFrameCount = value;
    return true;
}

bool pm::Settings::SetBufferFrameCount(uns32 value)
{
    m_bufferFrameCount = value;
    return true;
}

bool pm::Settings::SetBinningSerial(uns16 value)
{
    if (value == 0)
        return false;

    m_binSer = value;

    for (size_t n = 0; n < m_regions.size(); n++)
    {
        m_regions[n].sbin = m_binSer;
        m_regions[n].pbin = m_binPar;
    }

    return true;
}

bool pm::Settings::SetBinningParallel(uns16 value)
{
    if (value == 0)
        return false;

    m_binPar = value;

    for (size_t n = 0; n < m_regions.size(); n++)
    {
        m_regions[n].sbin = m_binSer;
        m_regions[n].pbin = m_binPar;
    }

    return true;
}

bool pm::Settings::SetRegions(const std::vector<rgn_type>& value)
{
    for (auto roi : value)
    {
        if (roi.sbin != m_binSer || roi.pbin != m_binPar)
        {
            Log::LogE("Region binning factors do not match");
            return false;
        }
    }

    m_regions = value;
    return true;
}

bool pm::Settings::SetExposure(uns32 value)
{
    m_expTime = value;
    return true;
}

bool pm::Settings::SetVtmExposures(const std::vector<uns16>& value)
{
    m_vtmExposures = value;
    return true;
}

bool pm::Settings::SetAcqMode(AcqMode value)
{
    m_acqMode = value;
    return true;
}

bool pm::Settings::SetTimeLapseDelay(unsigned int value)
{
    m_timeLapseDelay = value;
    return true;
}

bool pm::Settings::SetStorageType(StorageType value)
{
    m_storageType = value;
    return true;
}

bool pm::Settings::SetSaveDir(const std::string& value)
{
    m_saveDir = value;
    return true;
}

bool pm::Settings::SetSaveFirst(size_t value)
{
    m_saveFirst = value;
    return true;
}

bool pm::Settings::SetSaveLast(size_t value)
{
    m_saveLast = value;
    return true;
}

bool pm::Settings::SetMaxStackSize(size_t value)
{
    m_maxStackSize = value;
    return true;
}

bool pm::Settings::SetCentroidsEnabled(bool value)
{
    m_centroidsEnabled = value;
    return true;
}

bool pm::Settings::SetCentroidsCount(uns16 value)
{
    m_centroidsCount = value;
    return true;
}

bool pm::Settings::SetCentroidsRadius(uns16 value)
{
    m_centroidsRadius = value;
    return true;
}

bool pm::Settings::SetCentroidsMode(int32 value)
{
    m_centroidsMode = value;
    return true;
}

bool pm::Settings::SetCentroidsBackgroundCount(int32 value)
{
    m_centroidsBgCount = value;
    return true;
}

bool pm::Settings::SetCentroidsThreshold(uns32 value)
{
    m_centroidsThreshold = value;
    return true;
}

bool pm::Settings::SetTrackLinkFrames(uns16 value)
{
    m_trackLinkFrames = value;
    return true;
}

bool pm::Settings::SetTrackMaxDistance(uns16 value)
{
    m_trackMaxDistance = value;
    return true;
}

bool pm::Settings::SetTrackCpuOnly(bool value)
{
    m_trackCpuOnly = value;
    return true;
}

bool pm::Settings::SetTrackTrajectoryDuration(uns16 value)
{
    m_trackTrajectoryDuration = value;
    return true;
}
