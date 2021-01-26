#include "SettingsReader.h"

// The only place where are specified default values
pm::SettingsReader::SettingsReader()
    : m_camIndex(0), // First camera
    m_portIndex(0),
    m_speedIndex(0),
    m_gainIndex(1),
    m_emGainCapable(false),
    m_emGain(1),
    m_emGainMax(16384),
    m_bitDepth(16),
    m_width(0),
    m_height(0),
    m_clrCycles(2),
    m_clrMode(CLEAR_PRE_EXPOSURE),
    m_pMode(PMODE_NORMAL), // Should be PMODE_FT for frame-transfer camera
    m_trigMode(TIMED_MODE),
    m_expOutMode(EXPOSE_OUT_FIRST_ROW),
    m_circBufferCapable(false),
    m_metadataCapable(false),
    m_metadataEnabled(false),
    m_colorMask(COLOR_NONE),
    m_trigTabSignal(PL_TRIGTAB_SIGNAL_EXPOSE_OUT),
    m_lastMuxedSignal(1),
    m_expTimeRes(EXP_RES_ONE_MILLISEC),
    m_acqFrameCount(1),
    m_bufferFrameCount(0),
    m_binSer(1),
    m_binPar(1),
    m_regions(),
    m_regionCountMax(1),
    m_expTime(10),
    m_vtmExposures({ 10, 20, 30 }),
    m_acqMode(AcqMode::SnapSequence),
    m_timeLapseDelay(0),
    m_storageType(StorageType::None),
    m_saveDir(),
    m_saveFirst(0),
    m_saveLast(0),
    m_maxStackSize(0),
    m_centroidsCapable(false),
    m_centroidsEnabled(false),
    m_centroidsCount(100),
    m_centroidsCountMax(500),
    m_centroidsRadius(15),
    m_centroidsRadiusMax(15),
    m_centroidsModeCapable(false),
    m_centroidsMode(PL_CENTROIDS_MODE_LOCATE),
    m_centroidsBgCountCapable(false),
    m_centroidsBgCount(0),
    m_centroidsThresholdCapable(false),
    m_centroidsThreshold(160),
    m_trackLinkFrames(2),
    m_trackMaxDistance(25),
    m_trackCpuOnly(false),
    m_trackTrajectoryDuration(10)
{
}

pm::SettingsReader::~SettingsReader()
{
}

rgn_type pm::SettingsReader::GetImpliedRegion(const std::vector<rgn_type>& regions)
{
    rgn_type rgn{ 0, 0, 0, 0, 0, 0 };

    if (!regions.empty())
    {
        rgn_type impliedRgn = regions.at(0);
        for (size_t n = 1; n < regions.size(); n++)
        {
            const rgn_type& region = regions.at(n);

            if (impliedRgn.sbin != region.sbin || impliedRgn.pbin != region.pbin)
                return rgn;

            if (impliedRgn.s1 > region.s1)
                impliedRgn.s1 = region.s1;
            if (impliedRgn.s2 < region.s2)
                impliedRgn.s2 = region.s2;
            if (impliedRgn.p1 > region.p1)
                impliedRgn.p1 = region.p1;
            if (impliedRgn.p2 < region.p2)
                impliedRgn.p2 = region.p2;
        }

        rgn = impliedRgn;
    }

    return rgn;
}
