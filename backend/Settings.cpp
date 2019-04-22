#include "Settings.h"

/* Local */
#include "Log.h"
#include "Option.h"
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

bool pm::Settings::HandleCamIndex(const std::string& value)
{
    uns16 index;
    if (!StrToNumber<uns16>(value, index))
        return false;
    if (index >= std::numeric_limits<int16>::max())
        return false;

    return SetCamIndex((int16)index);
}

bool pm::Settings::HandlePortIndex(const std::string& value)
{
    uns32 portIndex;
    if (!StrToNumber<uns32>(value, portIndex))
        return false;

    return SetPortIndex(portIndex);
}

bool pm::Settings::HandleSpeedIndex(const std::string& value)
{
    int16 speedIndex;
    if (!StrToNumber<int16>(value, speedIndex))
        return false;

    return SetSpeedIndex(speedIndex);
}

bool pm::Settings::HandleGainIndex(const std::string& value)
{
    int16 gainIndex;
    if (!StrToNumber<int16>(value, gainIndex))
        return false;

    return SetGainIndex(gainIndex);
}

bool pm::Settings::HandleEmGain(const std::string& value)
{
    uns16 emGain;
    if (!StrToNumber<uns16>(value, emGain))
        return false;

    return SetEmGain(emGain);
}

bool pm::Settings::HandleClrCycles(const std::string& value)
{
    uns16 clrCycles;
    if (!StrToNumber<uns16>(value, clrCycles))
        return false;

    return SetClrCycles(clrCycles);
}

bool pm::Settings::HandleClrMode(const std::string& value)
{
    int32 clrMode;
    if (value == "never")
        clrMode = CLEAR_NEVER;
    else if (value == "pre-exp")
        clrMode = CLEAR_PRE_EXPOSURE;
    else if (value == "pre-seq")
        clrMode = CLEAR_PRE_SEQUENCE;
    else if (value == "post-seq")
        clrMode = CLEAR_POST_SEQUENCE;
    else if (value == "pre-post-seq")
        clrMode = CLEAR_PRE_POST_SEQUENCE;
    else if (value == "pre-exp-post-seq")
        clrMode = CLEAR_PRE_EXPOSURE_POST_SEQ;
    else
        return false;

    return SetClrMode(clrMode);
}

bool pm::Settings::HandlePMode(const std::string& value)
{
    int32 pMode;
    if (value == "normal")
        pMode = PMODE_NORMAL;
    else if (value == "ft")
        pMode = PMODE_FT;
    else if (value == "mpp")
        pMode = PMODE_MPP;
    else if (value == "ft-mpp")
        pMode = PMODE_FT_MPP;
    else if (value == "alt-normal")
        pMode = PMODE_ALT_NORMAL;
    else if (value == "alt-ft")
        pMode = PMODE_ALT_FT;
    else if (value == "alt-mpp")
        pMode = PMODE_ALT_MPP;
    else if (value == "alt-ft-mpp")
        pMode = PMODE_ALT_FT_MPP;
    else
        return false;

    return SetPMode(pMode);
}

bool pm::Settings::HandleTrigMode(const std::string& value)
{
    int32 trigMode;
    // Classic modes
    if (value == "timed")
        trigMode = TIMED_MODE;
    else if (value == "strobed")
        trigMode = STROBED_MODE;
    else if (value == "bulb")
        trigMode = BULB_MODE;
    else if (value == "trigger-first")
        trigMode = TRIGGER_FIRST_MODE;
    else if (value == "flash")
        trigMode = FLASH_MODE;
    else if (value == "variable-timed")
        trigMode = VARIABLE_TIMED_MODE;
    else if (value == "int-strobe")
        trigMode = INT_STROBE_MODE;
    // Extended modes
    else if (value == "ext-internal")
        trigMode = EXT_TRIG_INTERNAL;
    else if (value == "ext-trig-first")
        trigMode = EXT_TRIG_TRIG_FIRST;
    else if (value == "ext-edge-raising")
        trigMode = EXT_TRIG_EDGE_RISING;
    else
        return false;

    return SetTrigMode(trigMode);
}

bool pm::Settings::HandleExpOutMode(const std::string& value)
{
    int32 expOutMode;
    if (value == "first-row")
        expOutMode = EXPOSE_OUT_FIRST_ROW;
    else if (value == "all-rows")
        expOutMode = EXPOSE_OUT_ALL_ROWS;
    else if (value == "any-row")
        expOutMode = EXPOSE_OUT_ANY_ROW;
    else if (value == "rolling-shutter")
        expOutMode = EXPOSE_OUT_ROLLING_SHUTTER;
    else
        return false;

    return SetExpOutMode(expOutMode);
}

bool pm::Settings::HandleMetadataEnabled(const std::string& value)
{
    bool enabled;
    if (value.empty())
    {
        enabled = true;
    }
    else
    {
        if (!StrToBool(value, enabled))
            return false;
    }

    return SetMetadataEnabled(enabled);
}

bool pm::Settings::HandleTrigTabSignal(const std::string& value)
{
    int32 trigTabSignal;
    if (value == "expose-out")
        trigTabSignal = PL_TRIGTAB_SIGNAL_EXPOSE_OUT;
    else
        return false;

    return SetTrigTabSignal(trigTabSignal);
}

bool pm::Settings::HandleLastMuxedSignal(const std::string& value)
{
    uns8 lastMuxedSignal;
    if (!StrToNumber<uns8>(value, lastMuxedSignal))
        return false;

    return SetLastMuxedSignal(lastMuxedSignal);
}

bool pm::Settings::HandleAcqFrameCount(const std::string& value)
{
    uns32 acqFrameCount;
    if (!StrToNumber<uns32>(value, acqFrameCount))
        return false;

    return SetAcqFrameCount(acqFrameCount);
}

bool pm::Settings::HandleBufferFrameCount(const std::string& value)
{
    uns32 bufferFrameCount;
    if (!StrToNumber<uns32>(value, bufferFrameCount))
        return false;

    return SetBufferFrameCount(bufferFrameCount);
}

bool pm::Settings::HandleBinningSerial(const std::string& value)
{
    uns16 binSer;
    if (!StrToNumber<uns16>(value, binSer))
        return false;

    return SetBinningSerial(binSer);
}

bool pm::Settings::HandleBinningParallel(const std::string& value)
{
    uns16 binPar;
    if (!StrToNumber<uns16>(value, binPar))
        return false;

    return SetBinningParallel(binPar);
}

bool pm::Settings::HandleRegions(const std::string& value)
{
    std::vector<rgn_type> regions;

    if (value.empty())
    {
        // No regions means full sensor size will be used
        return SetRegions(regions);
    }

    const std::vector<std::string> rois =
        SplitString(value, Option::ValueGroupsSeparator);

    if (rois.size() == 0)
    {
        Log::LogE("Incorrect number of values for ROI");
        return false;
    }

    for (std::string roi : rois)
    {
        rgn_type rgn;
        rgn.sbin = m_binSer;
        rgn.pbin = m_binPar;

        const std::vector<std::string> values =
            SplitString(roi, Option::ValuesSeparator);

        if (values.size() != 4)
        {
            Log::LogE("Incorrect number of values for ROI");
            return false;
        }

        if (!StrToNumber<uns16>(values[0], rgn.s1)
                || !StrToNumber<uns16>(values[1], rgn.s2)
                || !StrToNumber<uns16>(values[2], rgn.p1)
                || !StrToNumber<uns16>(values[3], rgn.p2))
        {
            Log::LogE("Incorrect value(s) for ROI");
            return false;
        }

        regions.push_back(rgn);
    }

    return SetRegions(regions);
}

bool pm::Settings::HandleExposure(const std::string& value)
{
    const size_t suffixPos = value.find_first_not_of("0123456789");

    std::string rawValue(value);
    std::string suffix;
    if (suffixPos != std::string::npos)
    {
        suffix = value.substr(suffixPos);
        // Remove the suffix from value
        rawValue = value.substr(0, suffixPos);
    }

    uns32 expTime;
    if (!StrToNumber<uns32>(rawValue, expTime))
        return false;

    int32 expTimeRes;
    if (suffix == "us")
        expTimeRes = EXP_RES_ONE_MICROSEC;
    else if (suffix == "ms" || suffix.empty()) // ms is default resolution
        expTimeRes = EXP_RES_ONE_MILLISEC;
    else if (suffix == "s")
        expTimeRes = EXP_RES_ONE_SEC;
    else
        return false;

    if (!SetExposure(expTime))
        return false;
    return SetExposureResolution(expTimeRes);
}

bool pm::Settings::HandleVtmExposures(const std::string& value)
{
    std::vector<uns16> exposures;

    const std::vector<std::string> strExposures =
        SplitString(value, Option::ValuesSeparator);

    if (strExposures.size() == 0)
    {
        Log::LogE("Incorrect number of values for VTM exposures");
        return false;
    }

    uns16 exposure;
    for (std::string strExposure : strExposures)
    {
        if (!StrToNumber<uns16>(strExposure, exposure))
        {
            Log::LogE("VTM exposure value '%s' is not valid",
                    strExposure.c_str());
            return false;
        }
        if (exposure == 0)
        {
            Log::LogE("In VTM, zero exposure is not supported");
            return false;
        }

        exposures.push_back(exposure);
    }

    return SetVtmExposures(exposures);
}

bool pm::Settings::HandleAcqMode(const std::string& value)
{
    AcqMode acqMode;
    if (value == "snap-seq")
        acqMode = AcqMode::SnapSequence;
    else if (value == "snap-circ-buffer")
        acqMode = AcqMode::SnapCircBuffer;
    else if (value == "snap-time-lapse")
        acqMode = AcqMode::SnapTimeLapse;
    else if (value == "live-circ-buffer")
        acqMode = AcqMode::LiveCircBuffer;
    else if (value == "live-time-lapse")
        acqMode = AcqMode::LiveTimeLapse;
    else
        return false;

    return SetAcqMode(acqMode);
}

bool pm::Settings::HandleTimeLapseDelay(const std::string& value)
{
    unsigned int timeLapseDelay;
    if (!StrToNumber<unsigned int>(value, timeLapseDelay))
        return false;

    return SetTimeLapseDelay(timeLapseDelay);
}

bool pm::Settings::HandleStorageType(const std::string& value)
{
    StorageType storageType;
    if (value == "none")
        storageType = StorageType::None;
    else if (value == "prd")
        storageType = StorageType::Prd;
    else if (value == "tiff")
        storageType = StorageType::Tiff;
    else
        return false;

    return SetStorageType(storageType);
}

bool pm::Settings::HandleSaveDir(const std::string& value)
{
    return SetSaveDir(value);
}

bool pm::Settings::HandleSaveFirst(const std::string& value)
{
    size_t saveFirst;
    if (!StrToNumber<size_t>(value, saveFirst))
        return false;

    return SetSaveFirst(saveFirst);
}

bool pm::Settings::HandleSaveLast(const std::string& value)
{
    size_t saveLast;
    if (!StrToNumber<size_t>(value, saveLast))
        return false;

    return SetSaveLast(saveLast);
}

bool pm::Settings::HandleMaxStackSize(const std::string& value)
{
    const size_t suffixPos = value.find_last_of("kMG");
    char suffix = ' ';
    std::string rawValue(value);
    if (suffixPos != std::string::npos && suffixPos == value.length() - 1)
    {
        suffix = value[suffixPos];
        // Remove the suffix from value
        rawValue.pop_back();
    }

    size_t bytes;
    if (!StrToNumber<size_t>(rawValue, bytes))
        return false;
    size_t maxStackSize = bytes;

    const unsigned int maxBits = sizeof(size_t) * 8;

    unsigned int shiftBits = 0;
    if (suffix == 'G')
        shiftBits = 30;
    else if (suffix == 'M')
        shiftBits = 20;
    else if (suffix == 'k')
        shiftBits = 10;

    unsigned int currentBits = 1; // At least one bit
    while ((bytes >> currentBits) > 0)
        currentBits++;

    if (currentBits + shiftBits > maxBits)
    {
        Log::LogE("Value '%s' is too big (maxBits: %u, currentBits: %u, "
                "shiftBits: %u", value.c_str(), maxBits, currentBits, shiftBits);
        return false;
    }

    maxStackSize <<= shiftBits;

    return SetMaxStackSize(maxStackSize);
}

bool pm::Settings::HandleCentroidsEnabled(const std::string& value)
{
    bool enabled;
    if (value.empty())
    {
        enabled = true;
    }
    else
    {
        if (!StrToBool(value, enabled))
            return false;
    }

    return SetCentroidsEnabled(enabled);
}

bool pm::Settings::HandleCentroidsCount(const std::string& value)
{
    uns16 count;
    if (!StrToNumber<uns16>(value, count))
        return false;

    return SetCentroidsCount(count);
}

bool pm::Settings::HandleCentroidsRadius(const std::string& value)
{
    uns16 radius;
    if (!StrToNumber<uns16>(value, radius))
        return false;

    return SetCentroidsRadius(radius);
}

bool pm::Settings::HandleCentroidsMode(const std::string& value)
{
    int32 mode;
    if (value == "locate")
        mode = PL_CENTROIDS_MODE_LOCATE;
    else if (value == "track")
        mode = PL_CENTROIDS_MODE_TRACK;
    else
        return false;

    return SetCentroidsMode(mode);
}

bool pm::Settings::HandleCentroidsBackgroundCount(const std::string& value)
{
    uns16 bgCount16;
    if (!StrToNumber<uns16>(value, bgCount16))
        return false;

    return SetCentroidsBackgroundCount((int32)bgCount16);
}

bool pm::Settings::HandleCentroidsThreshold(const std::string& value)
{
    uns32 threshold;
    if (!StrToNumber<uns32>(value, threshold))
        return false;

    return SetCentroidsThreshold(threshold);
}

bool pm::Settings::HandleTrackLinkFrames(const std::string& value)
{
    uns16 frames;
    if (!StrToNumber<uns16>(value, frames))
        return false;

    return SetTrackLinkFrames(frames);
}

bool pm::Settings::HandleTrackMaxDistance(const std::string& value)
{
    uns16 maxDist;
    if (!StrToNumber<uns16>(value, maxDist))
        return false;

    return SetTrackMaxDistance(maxDist);
}

bool pm::Settings::HandleTrackCpuOnly(const std::string& value)
{
    bool cpuOnly;
    if (value.empty())
    {
        cpuOnly = false;
    }
    else
    {
        if (!StrToBool(value, cpuOnly))
            return false;
    }

    return SetTrackCpuOnly(cpuOnly);
}

bool pm::Settings::HandleTrackTrajectory(const std::string& value)
{
    uns16 duration;
    if (!StrToNumber<uns16>(value, duration))
        return false;

    return SetTrackTrajectoryDuration(duration);
}
