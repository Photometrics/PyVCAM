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

bool pm::Settings::AddOptions(OptionController& controller)
{
    const char valSep = pm::Option::ValuesSeparator;
    const char grpSep = pm::Option::ValueGroupsSeparator;

    if (!controller.AddOption(pm::Option(
            { "-CamIndex", "--cam-index", "-c" },
            { "index" },
            { "<camera default>" },
            "Index of camera to be used for acquisition.",
            static_cast<uint32_t>(OptionId::CamIndex),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleCamIndex),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-PortIndex", "--port-index" },
            { "index" },
            { "<camera default>" },
            "Port index (first is 0).",
            PARAM_READOUT_PORT,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandlePortIndex),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-SpeedIndex", "--speed-index" },
            { "index" },
            { "<camera default>" },
            "Speed index (first is 0).",
            PARAM_SPDTAB_INDEX,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleSpeedIndex),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-GainIndex", "--gain-index" },
            { "index" },
            { "<camera default>" },
            "Gain index (first is 1).",
            PARAM_GAIN_INDEX,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleGainIndex),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-EMGain", "--em-gain" },
            { "gain" },
            { "<camera default>" },
            "Gain multiplication factor for EM CCD cameras (lowest value is 1).",
            PARAM_GAIN_MULT_FACTOR,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleEmGain),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-ClearCycles", "--clear-cycles" },
            { "count" },
            { "<camera default>" },
            "Number of clear cycles.",
            PARAM_CLEAR_CYCLES,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleClrCycles),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-ClearMode", "--clear-mode" },
            { "mode" },
            { "<camera default>" },
            "Clear mode used for sensor clearing during acquisition.\n"
            "Supported values are : 'never', 'pre-exp', 'pre-seq', 'post-seq',\n"
            "'pre-post-seq' and 'pre-exp-post-seq'.",
            PARAM_CLEAR_MODE,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleClrMode),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-PMode", "--p-mode" },
            { "mode" },
            { "<camera default>" },
            "Parallel clocking mode used for sensor.\n"
            "Supported values are : 'normal', 'ft', 'mpp', 'ft-mpp', 'alt-normal',\n"
            "'alt-ft', 'alt-mpp' and 'alt-ft-mpp'.\n"
            "Modes with 'ft' in name are supported on frame-transfer capable cameras only.\n"
            "Modes with 'mpp' in name are supported on on MPP sensors only.\n"
            "Although the default value is 'normal', on frame-transfer cameras it should \n"
            "be 'ft' by default. Let's hope it won't cause problems.",
            PARAM_PMODE,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandlePMode),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-TrigMode", "--trig-mode" },
            { "mode" },
            { "<camera default>" },
            "Trigger (or exposure) mode used for exposure triggering.\n"
            "It is related to expose out mode, see details in PVCAM manual.\n"
            "Supported values are : Classics modes 'timed', 'strobed', 'bulb',\n"
            "'trigger-first', 'flash', 'variable-timed', 'int-strobe'\n"
            "and extended modes 'ext-internal', 'ext-trig-first' and 'ext-edge-raising'.\n"
            "WARNING:\n"
            "  'variable-timed' mode works in time-lapse acquisition modes only!",
            PARAM_EXPOSURE_MODE,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleTrigMode),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-ExpOutMode", "--exp-out-mode" },
            { "mode" },
            { "<camera default>" },
            "Expose mode used for exposure triggering.\n"
            "It is related to exposure mode, see details in PVCAM manual.\n"
            "Supported values are : 'first-row', 'all-rows', 'any-row' and 'rolling-shutter'.",
            PARAM_EXPOSE_OUT_MODE,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleExpOutMode),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-UseMetadata", "--use-metadata" },
            { "" },
            { "<camera default>" },
            "If camera supports frame metadata use it even if not needed.\n"
            "Application may silently override this value when metadata is needed,\n"
            "for instance multiple regions or centroids.",
            PARAM_METADATA_ENABLED,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleMetadataEnabled),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-TrigtabSignal", "--trigtab-signal" },
            { "signal" },
            { "<camera default>" },
            "The output signal with embedded multiplexer forwarding chosen signal\n"
            "to multiple output wires (set via --last-muxed-signal).\n"
            "Supported values are : 'expose-out'.",
            PARAM_TRIGTAB_SIGNAL,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleTrigTabSignal),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-LastMuxedSignal", "--last-muxed-signal" },
            { "number" },
            { "<camera default>" },
            "Number of multiplexed output wires for chosen signal (set via --trigtab-signal).",
            PARAM_LAST_MUXED_SIGNAL,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleLastMuxedSignal),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-AcqFrames", "--acq-frames", "-f" },
            { "count" },
            { "1" },
            "Total number of frames to be captured in acquisition.\n"
            "In snap sequence mode (set via --acq-mode) the total number of frames\n"
            "is limited to value 65535.",
            static_cast<uint32_t>(OptionId::AcqFrameCount),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleAcqFrameCount),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-BufferFrames", "--buffer-frames" },
            { "count" },
            { "10" },
            "Number of frames in PVCAM circular buffer.",
            static_cast<uint32_t>(OptionId::BufferFrameCount),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleBufferFrameCount),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-BinningSerial", "--binning-serial", "--sbin" },
            { "factor" },
            { "<camera default> or 1" },
            "Serial binning factor.",
            PARAM_BINNING_SER,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleBinningSerial),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-BinningParallel", "--binning-parallel", "--pbin" },
            { "factor" },
            { "<camera default> or 1" },
            "Parallel binning factor.",
            PARAM_BINNING_PAR,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleBinningParallel),
                    this, std::placeholders::_1))))
        return false;

    const std::string roiArgsDescs = std::string()
        + "sA1" + valSep + "sA2" + valSep + "pA1" + valSep + "pA2" + grpSep
        + "sB1" + valSep + "sB2" + valSep + "pB1" + valSep + "pB2" + grpSep
        + "...";
    if (!controller.AddOption(pm::Option(
            { "--region", "--regions", "--rois", "--roi", "-r" },
            { roiArgsDescs },
            { "" },
            "Region of interest for serial (width) and parallel (height) dimension.\n"
            "'sA1' is the first pixel, 'sA2' is the last pixel of the first region\n"
            "included on row. The same applies to columns. Multiple regions are\n"
            "separated by semicolon.\n"
            "Example of two diagonal regions 10x10: '--rois=0,9,0,9;10,19,10,19'.\n"
            "Binning factors are configured separately (via --sbin and --pbin).\n"
            "The empty value causes the camera's full-frame will be used internally.",
            static_cast<uint32_t>(OptionId::Regions),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleRegions),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-Exposure", "--exposure", "-e" },
            { "units" },
            { "10" },
            "Exposure time for each frame in millisecond units by default.\n"
            "Use us, ms or s suffix to change exposure resolution, e.g. 100us or 10ms.",
            static_cast<uint32_t>(OptionId::Exposure),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleExposure),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-VTMExposures", "--vtm-exposures" },
            { "units" },
            { "10,20,30" },
            "A set of exposure times used with variable timed trigger mode.\n"
            "It should be a list of comma-separated values in range from 1 to 65535.\n"
            "The exposure resolution is the same as set by --exposure option.\n"
            "WARNING:\n"
            "  VTM works in time-lapse acquisition modes only!",
            static_cast<uint32_t>(OptionId::VtmExposures),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleVtmExposures),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-AcqMode", "--acq-mode" },
            { "mode" },
            { "snap-seq" },
            "Specifies acquisition mode used for collecting images.\n"
            "Supported values are : 'snap-seq', 'snap-circ-buffer', 'snap-time-lapse',\n"
            "'live-circ-buffer' and 'live-time-lapse'.\n"
            "'snap-seq' mode:\n"
            "  Frames are captured in one sequence instead of continuous\n"
            "  acquisition with circular buffer.\n"
            "  Number of frames in buffer (set using --buffer-frames) has to\n"
            "  be equal or greater than number of frames in sequence\n"
            "  (set using --acq-frames).\n"
            "'snap-circ-buffer' mode:\n"
            "  Uses circular buffer to snap given number of frames in continuous\n"
            "  acquisition.\n"
            "  If the frame rate is high enough, it happens that number of\n"
            "  acquired frames is higher that requested, because new frames\n"
            "  can come between stop request and actual acq. interruption.\n"
            "'snap-time-lapse' mode:\n"
            "  Required number of frames is collected using multiple sequence\n"
            "  acquisitions where only one frame is captured at a time.\n"
            "  Delay between single frames can be set using --time-lapse-delay\n"
            "  option."
            "'live-circ-buffer' mode:\n"
            "  Uses circular buffer to snap frames in infinite continuous\n"
            "  acquisition.\n"
            "'live-time-lapse' mode:\n"
            "  The same as 'snap-time-lapse' but runs in infinite loop.",
            static_cast<uint32_t>(OptionId::AcqMode),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleAcqMode),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-TimeLapseDelay", "--time-lapse-delay" },
            { "milliseconds" },
            { "0" },
            "A delay between single frames in time lapse mode.",
            static_cast<uint32_t>(OptionId::TimeLapseDelay),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleTimeLapseDelay),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-SaveAs", "--save-as" },
            { "format" },
            { "none" },
            "Stores captured frames on disk in chosen format.\n"
            "Supported values are: 'none', 'prd' and 'tiff'.",
            static_cast<uint32_t>(OptionId::StorageType),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleStorageType),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-SaveDir", "--save-dir" },
            { "folder" },
            { "" },
            "Stores captured frames on disk in given directory.\n"
            "If empty string is given (the default) current working directory is used.",
            static_cast<uint32_t>(OptionId::SaveDir),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleSaveDir),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-SaveFirst", "--save-first" },
            { "count" },
            { "0" },
            "Saves only first <count> frames.\n"
            "If both --save-first and --save-last are zero, all frames are stored unless\n"
            "an option --save-as is 'none'.",
            static_cast<uint32_t>(OptionId::SaveFirst),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleSaveFirst),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-SaveLast", "--save-last" },
            { "count" },
            { "0" },
            "Saves only last <count> frames.\n"
            "If both --save-first and --save-last are zero, all frames are stored unless\n"
            "an option --save-as is 'none'.",
            static_cast<uint32_t>(OptionId::SaveLast),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleSaveLast),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-MaxStackSize", "--max-stack-size" },
            { "size" },
            { "0" },
            "Stores multiple frames in one file up to given size.\n"
            "Another stack file with new index is created for more frames.\n"
            "Use k, M or G suffix to enter nicer values. (1k = 1024)\n"
            "Default value is 0 which means each frame is stored to its own file.\n"
            "WARNING:\n"
            "  Storing too many small frames into one TIFF file (using --max-stack-size)\n"
            "  might be significantly slower compared to PRD format!",
            static_cast<uint32_t>(OptionId::MaxStackSize),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleMaxStackSize),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-UseCentroids", "--use-centroids" },
            { "" },
            { "<camera default>" },
            "Turns on the centroids feature.\n"
            "This feature can be used with up to one region only.",
            PARAM_CENTROIDS_ENABLED,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleCentroidsEnabled),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-CentroidsCount", "--centroids-count" },
            { "count" },
            { "<camera default>" },
            "Requests camera to find given number of centroids.\n"
            "Application may override this value if it is greater than max. number of\n"
            "supported centroids.",
            PARAM_CENTROIDS_COUNT,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleCentroidsCount),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-CentroidsRadius", "--centroids-radius" },
            { "radius" },
            { "<camera default>" },
            "Specifies the radius of all centroids.",
            PARAM_CENTROIDS_RADIUS,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleCentroidsRadius),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-CentroidsMode", "--centroids-mode" },
            { "mode" },
            { "<camera default>" },
            "Small objects can be either located only or tracked across frames.\n"
            "Supported values are : 'locate' and 'track'.",
            PARAM_CENTROIDS_MODE,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleCentroidsMode),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-CentroidsBgCount", "--centroids-bg-count" },
            { "frames" },
            { "<camera default>" },
            "Sets number of frames used for dynamic background removal.",
            PARAM_CENTROIDS_BG_COUNT,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleCentroidsBackgroundCount),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-CentroidsThreshold", "--centroids-threshold" },
            { "multiplier" },
            { "<camera default>" },
            "Sets a threshold multiplier. It is a fixed-point real number in format Q8.4.\n"
            "E.g. the value 1234 (0x4D2) means 77.2 (0x4D hex = 77 dec).",
            PARAM_CENTROIDS_THRESHOLD,
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleCentroidsThreshold),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-TrackLinkFrames", "--track-link-frames" },
            { "count" },
            { "10" },
            "Tracks particles for given number of frames.",
            static_cast<uint32_t>(OptionId::TrackLinkFrames),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleTrackLinkFrames),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-TrackMaxDist", "--track-max-dist" },
            { "pixels" },
            { "25" },
            "Searches for same particles not further than given distance.",
            static_cast<uint32_t>(OptionId::TrackMaxDistance),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleTrackMaxDistance),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-TrackCpuOnly", "--track-cpu-only" },
            { "" },
            { "false" },
            "Enforces linking on CPU, does not use CUDA on GPU even if available.",
            static_cast<uint32_t>(OptionId::TrackCpuOnly),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleTrackCpuOnly),
                    this, std::placeholders::_1))))
        return false;

    if (!controller.AddOption(pm::Option(
            { "-TrackTrajectory", "--track-trajectory" },
            { "frames" },
            { "10" },
            "Draws a trajectory lines for each particle for given number of frames.\n"
            "Zero value means the trajectories won't be displayed.",
            static_cast<uint32_t>(OptionId::TrackTrajectoryDuration),
            std::bind(static_cast<SettingsProto>(&pm::Settings::HandleTrackTrajectory),
                    this, std::placeholders::_1))))
        return false;

    return true;
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
