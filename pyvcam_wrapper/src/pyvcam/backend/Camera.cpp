#include "Camera.h"

/* System */
#include <limits>
#include <sstream>

/* Local */
#include "Frame.h"
#include "Log.h"
#include "Utils.h"

pm::Camera::Camera()
    : m_hCam(-1), // Invalid handle
    m_isOpen(false),
    m_isImaging(false),
    m_settings(),
    m_speeds(),
    m_frameAcqCfg(),
    m_frameCount(0),
    m_buffer(nullptr),
    m_frames(),
    m_framesMap()
{
}

pm::Camera::~Camera()
{
}

bool pm::Camera::ReviseSettings(Settings& settings)
{
    if (!m_isOpen)
        return false;

    std::vector<EnumItem> items;

    auto HasEnumItem = [](const std::vector<EnumItem>& items, int32 itemValue) {
        bool found = false;
        for (const EnumItem& item : items)
        {
            if (item.value == itemValue)
            {
                found = true;
                break;
            }
        }
        return found;
    };

    // Set port, speed and gain index first
    {
        int32 defVal;
        if (!GetParam(PARAM_READOUT_PORT, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default port index (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetPortIndex(defVal);
    }

    int32 portIndex = settings.GetPortIndex();
    if (!SetParam(PARAM_READOUT_PORT, &portIndex))
    {
        Log::LogE("Failure setting readout port index to %d (%s)", portIndex,
                GetErrorMessage().c_str());
        return false;
    }

    {
        int16 defVal;
        if (!GetParam(PARAM_SPDTAB_INDEX, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default speed index (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetSpeedIndex(defVal);
    }

    int16 speedIndex = settings.GetSpeedIndex();
    if (!SetParam(PARAM_SPDTAB_INDEX, &speedIndex))
    {
        Log::LogE("Failure setting speed index to %d (%s)", speedIndex,
                GetErrorMessage().c_str());
        return false;
    }

    {
        int16 defVal;
        if (!GetParam(PARAM_GAIN_INDEX, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default gain index (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetGainIndex(defVal);
    }

    int16 gainIndex = settings.GetGainIndex();
    if (!SetParam(PARAM_GAIN_INDEX, &gainIndex))
    {
        Log::LogE("Failure setting gain index to %d (%s)", gainIndex,
                GetErrorMessage().c_str());
        return false;
    }

    // Continue with all other parameters

    rs_bool hasEmGain;
    if (!GetParam(PARAM_GAIN_MULT_FACTOR, ATTR_AVAIL, &hasEmGain))
    {
        Log::LogE("Failure checking EM gain support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    settings.GetReadOnlyWriter().SetEmGainCapable(hasEmGain == TRUE);

    if (settings.GetEmGainCapable())
    {
        uns16 maxVal;
        if (!GetParam(PARAM_GAIN_MULT_FACTOR, ATTR_MAX, &maxVal))
        {
            Log::LogE("Failure getting max. EM gain (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.GetReadOnlyWriter().SetEmGainMax(maxVal);

        uns16 defVal;
        if (!GetParam(PARAM_GAIN_MULT_FACTOR, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default EM gain (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetEmGain(defVal);
    }

    uns16 bitDepth;
    if (!GetParam(PARAM_BIT_DEPTH, ATTR_CURRENT, &bitDepth))
    {
        Log::LogE("Failure getting bit depth (%s)", GetErrorMessage().c_str());
        return false;
    }
    settings.GetReadOnlyWriter().SetBitDepth(bitDepth);

    uns16 width;
    if (!GetParam(PARAM_SER_SIZE, ATTR_CURRENT, &width))
    {
        Log::LogE("Failure getting sensor width (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    settings.GetReadOnlyWriter().SetWidth(width);

    uns16 height;
    if (!GetParam(PARAM_PAR_SIZE, ATTR_CURRENT, &height))
    {
        Log::LogE("Failure getting sensor height (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    settings.GetReadOnlyWriter().SetHeight(height);

    {
        uns16 defVal;
        if (!GetParam(PARAM_CLEAR_CYCLES, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default clearing cycles (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetClrCycles(defVal);
    }

    {
        int32 defVal;
        if (!GetParam(PARAM_CLEAR_MODE, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default clearing mode (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetClrMode(defVal);
    }

    {
        int32 defVal;
        if (!GetParam(PARAM_PMODE, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default parallel clocking mode (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetPMode(defVal);
    }

    // A bit different handling, due to legacy and new modes a fix is forced
    {
        int32 defVal;
        if (!GetParam(PARAM_EXPOSURE_MODE, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default triggering mode (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetTrigMode(defVal);
    }

    rs_bool hasExpOutModes;
    if (!GetParam(PARAM_EXPOSE_OUT_MODE, ATTR_AVAIL, &hasExpOutModes))
    {
        Log::LogE("Failure getting expose out modes availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    if (hasExpOutModes)
    {
        int32 defVal;
        if (!GetParam(PARAM_EXPOSE_OUT_MODE, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default expose out mode (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetExpOutMode(defVal);
    }

    rs_bool hasCircBuf;
    if (!GetParam(PARAM_CIRC_BUFFER, ATTR_AVAIL, &hasCircBuf))
    {
        Log::LogE("Failure checking circular buffer support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    settings.GetReadOnlyWriter().SetCircBufferCapable(hasCircBuf == TRUE);

    rs_bool hasMetadata;
    if (!GetParam(PARAM_METADATA_ENABLED, ATTR_AVAIL, &hasMetadata))
    {
        Log::LogE("Failure checking frame metadata support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    settings.GetReadOnlyWriter().SetMetadataCapable(hasMetadata == TRUE);

    if (!settings.GetMetadataCapable())
    {
        if (settings.GetMetadataEnabled())
        {
            settings.SetMetadataEnabled(false);
        }
    }

    // For monochromatic cameras the parameter might not be available
    int32 colorMask = COLOR_NONE;
    rs_bool hasColorMask;
    if (!GetParam(PARAM_COLOR_MODE, ATTR_AVAIL, &hasColorMask))
    {
        Log::LogE("Failure getting color mask support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (hasColorMask)
    {
        if (!GetParam(PARAM_COLOR_MODE, ATTR_CURRENT, &colorMask))
        {
            Log::LogE("Failure getting color mask (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
    }
    settings.GetReadOnlyWriter().SetColorMask(colorMask);

    rs_bool hasTrigTabSignal;
    if (!GetParam(PARAM_TRIGTAB_SIGNAL, ATTR_AVAIL, &hasTrigTabSignal))
    {
        Log::LogE("Failure getting trigger table signals availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    // TODO: settings.GetReadOnlyWriter().SetTrigTabSignalCapable(hasTrigTabSignal);

    if (hasTrigTabSignal)
    {
        int32 defVal;
        if (!GetParam(PARAM_TRIGTAB_SIGNAL, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default trigger table signal (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetTrigTabSignal(defVal);
    }

    rs_bool hasLastMuxedSignal;
    if (!GetParam(PARAM_LAST_MUXED_SIGNAL, ATTR_AVAIL, &hasLastMuxedSignal))
    {
        Log::LogE("Failure getting last multiplexed signal availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    // TODO: settings.GetReadOnlyWriter().SetLastMuxedSignalCapable(hasLastMuxedSignal);

    if (hasLastMuxedSignal)
    {
        uns8 defVal;
        if (!GetParam(PARAM_LAST_MUXED_SIGNAL, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default last multiplexed signal (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetLastMuxedSignal(defVal);
    }

    rs_bool hasExpRes;
    if (!GetParam(PARAM_EXP_RES, ATTR_AVAIL, &hasExpRes))
    {
        Log::LogE("Failure getting exposure resolution availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    // Parameter can be overriden only together with exposure time
    if (hasExpRes)
    {
        int32 defVal;
        if (!GetParam(PARAM_EXP_RES, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default exposure resolution (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetExposureResolution(defVal);
    }
    else
    {
        int32 defVal = EXP_RES_ONE_MILLISEC;
        settings.SetExposureResolution(defVal);
    }

    rs_bool hasBinSer;
    if (!GetParam(PARAM_BINNING_SER, ATTR_AVAIL, &hasBinSer))
    {
        Log::LogE("Failure getting serial binning factors availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    rs_bool hasBinPar;
    if (!GetParam(PARAM_BINNING_PAR, ATTR_AVAIL, &hasBinPar))
    {
        Log::LogE("Failure getting parallel binning factors availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (hasBinSer && hasBinPar)
    {
        int32 defSerVal;
        if (!GetParam(PARAM_BINNING_SER, ATTR_DEFAULT, &defSerVal))
        {
            Log::LogE("Failure getting default serial binning factor (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        int32 defParVal;
        if (!GetParam(PARAM_BINNING_PAR, ATTR_DEFAULT, &defParVal))
        {
            Log::LogE("Failure getting default parallel binning factor (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetBinningSerial((uns16)defSerVal);
        settings.SetBinningParallel((uns16)defParVal);
    }

    // Older PVCAMs don't have this parameter yet
    uns16 regionCountMax = 1;
    rs_bool hasRoiCount;
    if (!GetParam(PARAM_ROI_COUNT, ATTR_AVAIL, &hasRoiCount))
    {
        Log::LogE("Failure getting region count availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (hasRoiCount)
    {
        if (!GetParam(PARAM_ROI_COUNT, ATTR_MAX, &regionCountMax))
        {
            Log::LogE("Failure getting max. ROI count (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
    }
    settings.GetReadOnlyWriter().SetRegionCountMax(regionCountMax);

    std::vector<rgn_type> regions = settings.GetRegions();
    if (regions.size() > regionCountMax)
    {
        regions.erase(regions.begin() + regionCountMax, regions.end());
        // Cannot fail, remaining regions already were in settings so are valid
        settings.SetRegions(regions);
    }

    rs_bool hasCentroidsEnabled;
    if (!GetParam(PARAM_CENTROIDS_ENABLED, ATTR_AVAIL, &hasCentroidsEnabled))
    {
        Log::LogE("Failure checking centroids support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    rs_bool hasCentroidsCount;
    if (!GetParam(PARAM_CENTROIDS_COUNT, ATTR_AVAIL, &hasCentroidsCount))
    {
        Log::LogE("Failure checking centroids count support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    rs_bool hasCentroidsRadius;
    if (!GetParam(PARAM_CENTROIDS_RADIUS, ATTR_AVAIL, &hasCentroidsRadius))
    {
        Log::LogE("Failure checking centroids radius support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    const bool isCentroidsCapable = hasCentroidsEnabled == TRUE
        && hasCentroidsCount == TRUE && hasCentroidsRadius == TRUE;
    settings.GetReadOnlyWriter().SetCentroidsCapable(isCentroidsCapable);

    if (!settings.GetCentroidsCapable())
    {
        if (settings.GetCentroidsEnabled())
        {
            settings.SetCentroidsEnabled(false);
        }
    }
    else
    {

        {
            rs_bool defVal;
            if (!GetParam(PARAM_CENTROIDS_ENABLED, ATTR_DEFAULT, &defVal))
            {
                Log::LogE("Failure getting default centroids enabled state (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            settings.SetCentroidsEnabled(defVal == TRUE);
        }

        uns16 centroidsCountMin;
        if (!GetParam(PARAM_CENTROIDS_COUNT, ATTR_MIN, &centroidsCountMin))
        {
            Log::LogE("Failure getting min. centroids count (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        // TODO: settings.GetReadOnlyWriter().SetCentroidsCountMin(centroidsCountMin);
        uns16 centroidsCountMax;
        if (!GetParam(PARAM_CENTROIDS_COUNT, ATTR_MAX, &centroidsCountMax))
        {
            Log::LogE("Failure getting max. centroids count (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.GetReadOnlyWriter().SetCentroidsCountMax(centroidsCountMax);

        uns16 defVal;
        if (!GetParam(PARAM_CENTROIDS_COUNT, ATTR_DEFAULT, &defVal))
        {
            Log::LogE("Failure getting default centroids count (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.SetCentroidsCount(defVal);

        uns16 centroidsRadiusMin;
        if (!GetParam(PARAM_CENTROIDS_RADIUS, ATTR_MIN, &centroidsRadiusMin))
        {
            Log::LogE("Failure getting min. centroids radius (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        // TODO: settings.GetReadOnlyWriter().SetCentroidsRadiusMin(centroidsRadiusMin);
        uns16 centroidsRadiusMax;
        if (!GetParam(PARAM_CENTROIDS_RADIUS, ATTR_MAX, &centroidsRadiusMax))
        {
            Log::LogE("Failure getting max. centroids radius (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.GetReadOnlyWriter().SetCentroidsRadiusMax(centroidsRadiusMax);

        {
            uns16 defVal;
            if (!GetParam(PARAM_CENTROIDS_RADIUS, ATTR_DEFAULT, &defVal))
            {
                Log::LogE("Failure getting default centroids radius (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            settings.SetCentroidsRadius(defVal);
        }

        rs_bool hasCentroidsMode;
        if (!GetParam(PARAM_CENTROIDS_MODE, ATTR_AVAIL, &hasCentroidsMode))
        {
            Log::LogE("Failure checking centroids mode support (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.GetReadOnlyWriter().SetCentroidsModeCapable(hasCentroidsMode == TRUE);

        if (hasCentroidsMode)
        {
            int32 defVal;
            if (!GetParam(PARAM_CENTROIDS_MODE, ATTR_DEFAULT, &defVal))
            {
                Log::LogE("Failure getting default centroids mode (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            settings.SetCentroidsMode(defVal);
        }

        rs_bool hasCentroidsBgCount;
        if (!GetParam(PARAM_CENTROIDS_BG_COUNT, ATTR_AVAIL, &hasCentroidsBgCount))
        {
            Log::LogE("Failure checking centroids background count support (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.GetReadOnlyWriter().SetCentroidsBgCountCapable(hasCentroidsBgCount == TRUE);

        if (hasCentroidsBgCount)
        {
            int32 defVal;
            if (!GetParam(PARAM_CENTROIDS_BG_COUNT, ATTR_DEFAULT, &defVal))
            {
                Log::LogE("Failure getting default centroids background count (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            settings.SetCentroidsBackgroundCount(defVal);
        }

        rs_bool hasCentroidsThreshold;
        if (!GetParam(PARAM_CENTROIDS_THRESHOLD, ATTR_AVAIL, &hasCentroidsThreshold))
        {
            Log::LogE("Failure checking centroids threshold support (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        settings.GetReadOnlyWriter().SetCentroidsThresholdCapable(hasCentroidsThreshold == TRUE);

        if (hasCentroidsThreshold)
        {
            uns32 centroidsThresholdMin;
            if (!GetParam(PARAM_CENTROIDS_THRESHOLD, ATTR_MIN, &centroidsThresholdMin))
            {
                Log::LogE("Failure getting min. centroids threshold (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            // TODO: settings.GetReadOnlyWriter().SetCentroidsThresholdMin(centroidsThresholdMin);

            uns32 centroidsThresholdMax;
            if (!GetParam(PARAM_CENTROIDS_THRESHOLD, ATTR_MAX, &centroidsThresholdMax))
            {
                Log::LogE("Failure getting max. centroids threshold (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            // TODO: settings.GetReadOnlyWriter().SetCentroidsThresholdMax(centroidsThresholdMax);

            uns16 defVal;
            if (!GetParam(PARAM_CENTROIDS_THRESHOLD, ATTR_DEFAULT, &defVal))
            {
                Log::LogE("Failure getting default centroids threshold raw (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            settings.SetCentroidsThreshold(defVal);
        }
    }

    // Enforcing frame metadata usage when needed, do not fail
    if (!settings.GetMetadataEnabled())
    {
        if (settings.GetCentroidsCapable() && settings.GetCentroidsEnabled())
        {
            Log::LogW("Enforcing frame metadata usage with centroids");
            settings.SetMetadataEnabled(true);
        }
        if (settings.GetRegions().size() > 1)
        {
            Log::LogW("Enforcing frame metadata usage with multiple regions");
            settings.SetMetadataEnabled(true);
        }
    }

    // Print some info about camera

    rs_bool hasProductName;
    if (!GetParam(PARAM_PRODUCT_NAME, ATTR_AVAIL, &hasProductName))
    {
        Log::LogE("Failure getting product name availability (%s)",
                 GetErrorMessage().c_str());
        return false;
    }
    // We can continue without product name
    if (hasProductName)
    {
        char name[MAX_PRODUCT_NAME_LEN];
        if (!GetParam(PARAM_PRODUCT_NAME, ATTR_CURRENT, &name))
        {
            Log::LogE("Failure getting product name (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        Log::LogI("Product: '%s'", name);
    }

    Log::LogI("Sensor resolution: %ux%u px",
            settings.GetWidth(), settings.GetHeight());

    rs_bool hasChipName;
    if (!GetParam(PARAM_CHIP_NAME, ATTR_AVAIL, &hasChipName))
    {
        Log::LogE("Failure getting chip name availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    // We can continue without chip name
    if (hasChipName)
    {
        char chipName[CCD_NAME_LEN];
        if (!GetParam(PARAM_CHIP_NAME, ATTR_CURRENT, &chipName))
        {
            Log::LogE("Failure getting sensor name (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        Log::LogI("Sensor name: '%s'", chipName);
    }
    else
    {
        Log::LogW("Sensor name: NOT SUPPORTED");
    }

    rs_bool hasSerNum;
    if (!GetParam(PARAM_HEAD_SER_NUM_ALPHA, ATTR_AVAIL, &hasSerNum))
    {
        Log::LogE("Failure getting serial number availability (%s)",
                 GetErrorMessage().c_str());
        return false;
    }
    // We can continue without serial number
    if (hasSerNum)
    {
        char serNum[MAX_ALPHA_SER_NUM_LEN];
        if (!GetParam(PARAM_HEAD_SER_NUM_ALPHA, ATTR_CURRENT, &serNum))
        {
            Log::LogE("Failure getting serial number (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        Log::LogI("Serial number: '%s'", serNum);
    }
    else
    {
        Log::LogW("Serial number: NOT SUPPORTED");
    }

    rs_bool hasIfcTypes;
    if (!GetParam(PARAM_CAM_INTERFACE_TYPE, ATTR_AVAIL, &hasIfcTypes))
    {
        Log::LogE("Failure getting interface types availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    // We can continue without interface types
    if (hasIfcTypes)
    {
        if (!GetEnumParam(PARAM_CAM_INTERFACE_TYPE, items))
        {
            Log::LogE("Failure getting interface types (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        int32 curType;
        if (!GetParam(PARAM_CAM_INTERFACE_TYPE, ATTR_CURRENT, &curType))
        {
            Log::LogE("Failure getting current interface type (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        const EnumItem* curTypeItem = nullptr;
        for (const EnumItem& item : items)
        {
            if (item.value == curType)
            {
                curTypeItem = &item;
                break;
            }
        }
        if (curTypeItem != nullptr)
        {
            Log::LogI("Interface type: '%s'", curTypeItem->desc.c_str());
        }
        else
        {
            Log::LogW("Interface type: UNKNOWN");
        }
    }

    rs_bool hasIfcModes;
    if (!GetParam(PARAM_CAM_INTERFACE_MODE, ATTR_AVAIL, &hasIfcModes))
    {
        Log::LogE("Failure getting interface modes availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    // We can continue without interface modes
    if (hasIfcModes)
    {
        int32 curIfcMode;
        if (!GetParam(PARAM_CAM_INTERFACE_MODE, ATTR_CURRENT, &curIfcMode))
        {
            Log::LogE("Failure getting current interface mode (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        if (curIfcMode != PL_CAM_IFC_MODE_IMAGING)
        {
            Log::LogE("Current interface mode is not sufficient for imaging");
            return false;
        }
    }

    static const std::map<int32, const char*> colorMaskNameMap({
        { COLOR_NONE, "None" },
        { COLOR_RGGB, "RGGB" },
        { COLOR_GRBG, "GRBG" },
        { COLOR_GBRG, "GBRG" },
        { COLOR_BGGR, "BGGR" },
    });
    if (colorMaskNameMap.count(colorMask))
    {
        Log::LogI("Color mask: %s", colorMaskNameMap.at(colorMask));
    }
    else
    {
        Log::LogW("Color mask: UNKNOWN");
    }

    uns16 fwVer;
    if (!GetParam(PARAM_CAM_FW_VERSION, ATTR_CURRENT, &fwVer))
    {
        Log::LogE("Failure getting camera firmware version (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    Log::LogI("Firmware version: %u.%u\n", (fwVer >> 8) & 0xFF, fwVer & 0xFF);

    return true;
}

bool pm::Camera::Open(const std::string& /*name*/)
{
    if (!BuildSpeedTable())
        return false;

    m_isOpen = true;
    return true;
}

bool pm::Camera::Close()
{
    m_speeds.clear();

    m_isOpen = false;
    return true;
}

bool pm::Camera::SetupExp(const SettingsReader& settings)
{
    m_settings = settings;

    if (m_settings.GetRegions().size() > m_settings.GetRegionCountMax()
            || m_settings.GetRegions().empty())
    {
        Log::LogE("Invalid number of regions (%llu)",
                (unsigned long long)m_settings.GetRegions().size());
        return false;
    }

    const uns32 acqFrameCount = m_settings.GetAcqFrameCount();
    const uns32 bufferFrameCount = m_settings.GetBufferFrameCount();
    const AcqMode acqMode = m_settings.GetAcqMode();
    const int32 trigMode = m_settings.GetTrigMode();

    if (acqMode == AcqMode::SnapSequence && acqFrameCount > bufferFrameCount)
    {
        Log::LogE("When in snap sequence mode, "
                "we cannot acquire more frames than the buffer size (%u)",
                bufferFrameCount);
        return false;
    }

    if ((acqMode == AcqMode::LiveCircBuffer || acqMode == AcqMode::LiveTimeLapse)
            && m_settings.GetStorageType() != StorageType::None
            && m_settings.GetSaveLast() > 0)
    {
        Log::LogE("When in live mode, we cannot save last N frames");
        return false;
    }

    if (acqMode != AcqMode::SnapTimeLapse && acqMode != AcqMode::LiveTimeLapse
            && trigMode == VARIABLE_TIMED_MODE)
    {
        Log::LogE("'Variable Timed' mode works in time-lapse modes only");
        return false;
    }

    // Set port, speed and gain index first
    int32 portIndex = m_settings.GetPortIndex();
    if (!SetParam(PARAM_READOUT_PORT, &portIndex))
    {
        Log::LogE("Failure setting readout port index to %d (%s)", portIndex,
                GetErrorMessage().c_str());
        return false;
    }
    int16 speedIndex = m_settings.GetSpeedIndex();
    if (!SetParam(PARAM_SPDTAB_INDEX, &speedIndex))
    {
        Log::LogE("Failure setting speed index to %d (%s)", speedIndex,
                GetErrorMessage().c_str());
        return false;
    }
    int16 gainIndex = m_settings.GetGainIndex();
    if (!SetParam(PARAM_GAIN_INDEX, &gainIndex))
    {
        Log::LogE("Failure setting gain index to %d (%s)", gainIndex,
                GetErrorMessage().c_str());
        return false;
    }

    if (m_settings.GetEmGainCapable())
    {
        uns16 emGain = m_settings.GetEmGain();
        if (!SetParam(PARAM_GAIN_MULT_FACTOR, &emGain))
        {
            Log::LogE("Failure setting EM gain to %u (%s)", emGain,
                    GetErrorMessage().c_str());
            return false;
        }
    }

    uns16 clrCycles = m_settings.GetClrCycles();
    if (!SetParam(PARAM_CLEAR_CYCLES, &clrCycles))
    {
        Log::LogE("Failure setting clear cycles to %u (%s)", clrCycles,
                GetErrorMessage().c_str());
        return false;
    }
    int32 clrMode = m_settings.GetClrMode();
    if (!SetParam(PARAM_CLEAR_MODE, &clrMode))
    {
        Log::LogE("Failure setting clearing mode to %d (%s)", clrMode,
                GetErrorMessage().c_str());
        return false;
    }

    int32 pMode = m_settings.GetPMode();
    if (!SetParam(PARAM_PMODE, &pMode))
    {
        Log::LogE("Failure setting clocking mode to %d (%s)", pMode,
                GetErrorMessage().c_str());
        return false;
    }

    if (m_settings.GetMetadataCapable())
    {
        rs_bool enabled = m_settings.GetMetadataEnabled();
        if (!SetParam(PARAM_METADATA_ENABLED, &enabled))
        {
            if (enabled)
            {
                Log::LogE("Failure enabling frame metadata (%s)",
                        GetErrorMessage().c_str());
            }
            else
            {
                Log::LogE("Failure disabling frame metadata (%s)",
                        GetErrorMessage().c_str());
            }
            return false;
        }
    }
    else
    {
        if (m_settings.GetMetadataEnabled())
        {
            Log::LogE("Unable to enable frame metadata, camera does not support it");
            return false;
        }
    }

    rs_bool hasTrigTabSignal;
    if (!GetParam(PARAM_TRIGTAB_SIGNAL, ATTR_AVAIL, &hasTrigTabSignal))
    {
        Log::LogE("Failure getting trigger table signals availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (hasTrigTabSignal)
    {
        int32 trigTabSignal = m_settings.GetTrigTabSignal();
        if (!SetParam(PARAM_TRIGTAB_SIGNAL, &trigTabSignal))
        {
            Log::LogE("Failure setting triggering table signal to %d (%s)",
                    trigTabSignal, GetErrorMessage().c_str());
            return false;
        }
    }
    rs_bool hasLastMuxedSignal;
    if (!GetParam(PARAM_LAST_MUXED_SIGNAL, ATTR_AVAIL, &hasLastMuxedSignal))
    {
        Log::LogE("Failure getting last muxed signal availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (hasLastMuxedSignal)
    {
        uns8 lastMuxedSignal = m_settings.GetLastMuxedSignal();
        if (!SetParam(PARAM_LAST_MUXED_SIGNAL, &lastMuxedSignal))
        {
            Log::LogE("Failure setting last multiplexed signal to %u (%s)",
                    lastMuxedSignal, GetErrorMessage().c_str());
            return false;
        }
    }

    int32 expTimeRes = m_settings.GetExposureResolution();
    const char* resName = "<UNKNOWN>";
    switch (expTimeRes)
    {
    case EXP_RES_ONE_MICROSEC:
        resName = "microseconds";
        break;
    case EXP_RES_ONE_MILLISEC:
        resName = "milliseconds";
        break;
    case EXP_RES_ONE_SEC:
        resName = "seconds";
        break;
    }
    rs_bool hasExpRes;
    if (!GetParam(PARAM_EXP_RES, ATTR_AVAIL, &hasExpRes))
    {
        Log::LogE("Failure getting exposure resolutions availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    // Parameter not available, camera supports only milliseconds resolution
    if (hasExpRes)
    {
        // PARAM_EXP_RES was read-only in older PVCAMs, use the other one
        if (!SetParam(PARAM_EXP_RES_INDEX, &expTimeRes))
        {
            Log::LogE("Failure setting exposure resolution to %s (%d) (%s)",
                    resName, expTimeRes, GetErrorMessage().c_str());
            return false;
        }
    }

    if (m_settings.GetCentroidsCapable())
    {
        rs_bool enabled = m_settings.GetCentroidsEnabled();
        if (!SetParam(PARAM_CENTROIDS_ENABLED, &enabled))
        {
            if (enabled)
            {
                Log::LogE("Failure enabling centroids (%s)",
                        GetErrorMessage().c_str());
            }
            else
            {
                Log::LogE("Failure disabling centroids (%s)",
                        GetErrorMessage().c_str());
            }
            return false;
        }
        if (enabled)
        {
            uns16 count = m_settings.GetCentroidsCount();
            if (!SetParam(PARAM_CENTROIDS_COUNT, &count))
            {
                Log::LogE("Failure setting centroid count to %u (%s)", count,
                        GetErrorMessage().c_str());
                return false;
            }

            uns16 radius = m_settings.GetCentroidsRadius();
            if (!SetParam(PARAM_CENTROIDS_RADIUS, &radius))
            {
                Log::LogE("Failure setting centroid radius (%s)",
                        GetErrorMessage().c_str());
                return false;
            }

            if (m_settings.GetCentroidsModeCapable())
            {
                int32 mode = m_settings.GetCentroidsMode();
                if (!SetParam(PARAM_CENTROIDS_MODE, &mode))
                {
                    Log::LogE("Failure setting centroids mode to %d (%s)", mode,
                            GetErrorMessage().c_str());
                    return false;
                }
            }

            if (m_settings.GetCentroidsBackgroundCountCapable())
            {
                int32 bgCount = m_settings.GetCentroidsBackgroundCount();
                if (!SetParam(PARAM_CENTROIDS_BG_COUNT, &bgCount))
                {
                    Log::LogE("Failure setting centroids background count to %d (%s)",
                            bgCount, GetErrorMessage().c_str());
                    return false;
                }
            }

            if (m_settings.GetCentroidsThresholdCapable())
            {
                uns32 threshold = m_settings.GetCentroidsThreshold();
                if (!SetParam(PARAM_CENTROIDS_THRESHOLD, &threshold))
                {
                    Log::LogE("Failure setting centroids threshold multiplier to %u (%s)",
                            threshold, GetErrorMessage().c_str());
                    return false;
                }
            }
        }
    }
    else
    {
        if (m_settings.GetCentroidsEnabled())
        {
            Log::LogE("Unable to enable centroids, camera does not support it");
            return false;
        }
    }

    // Setup the acquisition and call AllocateBuffer in derived class

    return true;
}

std::shared_ptr<pm::Frame> pm::Camera::GetFrameAt(size_t index) const
{
    if (index >= m_frames.size())
    {
        Log::LogD("Frame index out of buffer boundaries");
        return nullptr;
    }

    return m_frames.at(index);
}

bool pm::Camera::GetFrameIndex(const Frame& frame, size_t& index) const
{
    const uint32_t frameNr = frame.GetInfo().GetFrameNr();

    auto it = m_framesMap.find(frameNr);
    if (it != m_framesMap.end())
    {
        index = it->second;
        return true;
    }

    return false;
}

void pm::Camera::UpdateFrameIndexMap(uint32_t oldFrameNr, size_t index) const
{
    m_framesMap.erase(oldFrameNr);

    if (index >= m_frames.size())
        return;
    const uint32_t frameNr = m_frames.at(index)->GetInfo().GetFrameNr();
    m_framesMap[frameNr] = index;
}

bool pm::Camera::BuildSpeedTable()
{
    m_speeds.clear();

    rs_bool hasReadoutPorts;
    if (!GetParam(PARAM_READOUT_PORT, ATTR_AVAIL, &hasReadoutPorts))
    {
        Log::LogE("Failure getting readout ports availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (!hasReadoutPorts)
    {
        Log::LogE("Readout ports not available");
        return false;
    }

    rs_bool hasSpeedIdx;
    if (!GetParam(PARAM_SPDTAB_INDEX, ATTR_AVAIL, &hasSpeedIdx))
    {
        Log::LogE("Failure getting speed indices availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (!hasSpeedIdx)
    {
        Log::LogE("Speed indices not available");
        return false;
    }

    rs_bool hasGainIdx;
    if (!GetParam(PARAM_GAIN_INDEX, ATTR_AVAIL, &hasGainIdx))
    {
        Log::LogE("Failure getting gain indices availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (!hasGainIdx)
    {
        Log::LogE("Gain indices not available");
        return false;
    }

    rs_bool hasGainName;
    if (!GetParam(PARAM_GAIN_NAME, ATTR_AVAIL, &hasGainName))
    {
        Log::LogE("Failure checking gain name support (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    // Gain name is optional

    rs_bool hasBitDepth;
    if (!GetParam(PARAM_BIT_DEPTH, ATTR_AVAIL, &hasBitDepth))
    {
        Log::LogE("Failure getting bit depth availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (!hasBitDepth)
    {
        Log::LogE("Bit depth not available");
        return false;
    }

    rs_bool hasPixTime;
    if (!GetParam(PARAM_PIX_TIME, ATTR_AVAIL, &hasPixTime))
    {
        Log::LogE("Failure getting pixel time availability (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    if (!hasPixTime)
    {
        Log::LogE("Pixel time not available");
        return false;
    }

    std::vector<EnumItem> portItems;
    if (!GetEnumParam(PARAM_READOUT_PORT, portItems))
    {
        Log::LogE("Failure getting readout ports (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    for (EnumItem portItem : portItems)
    {
        if (!SetParam(PARAM_READOUT_PORT, &portItem.value))
        {
            Log::LogE("Failure setting readout port index to %d (%s)",
                    portItem.value, GetErrorMessage().c_str());
            return false;
        }

        uns32 speedCount;
        if (!GetParam(PARAM_SPDTAB_INDEX, ATTR_COUNT, &speedCount))
        {
            Log::LogE("Failure getting speed count (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        for (int16 speedIndex = 0; speedIndex < (int16)speedCount; speedIndex++)
        {
            Speed speed;

            if (!SetParam(PARAM_SPDTAB_INDEX, &speedIndex))
            {
                Log::LogE("Failure setting speed index to %d (%s)",
                        speedIndex, GetErrorMessage().c_str());
                return false;
            }

            uns16 bitDepth;
            if (!GetParam(PARAM_BIT_DEPTH, ATTR_CURRENT, &bitDepth))
            {
                Log::LogE("Failure getting sensor bit depth (%s)",
                        GetErrorMessage().c_str());
                return false;
            }

            uns16 pixTimeNs;
            if (!GetParam(PARAM_PIX_TIME, ATTR_CURRENT, &pixTimeNs))
            {
                Log::LogE("Failure getting pixel readout time (%s)",
                        GetErrorMessage().c_str());
                return false;
            }

            std::vector<EnumItem> gains;
            int16 gainIndexMax;
            if (!GetParam(PARAM_GAIN_INDEX, ATTR_MAX, &gainIndexMax))
            {
                Log::LogE("Failure getting max. gain index (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
            for (int16 gainIndex = 1; gainIndex <= gainIndexMax; gainIndex++)
            {
                EnumItem gain;
                gain.value = gainIndex;
                if (hasGainName)
                {
                    if (!SetParam(PARAM_GAIN_INDEX, &gainIndex))
                    {
                        Log::LogE("Failure setting gain index to %d (%s)",
                                gainIndex, GetErrorMessage().c_str());
                        return false;
                    }
                    char gainName[MAX_GAIN_NAME_LEN];
                    if (!GetParam(PARAM_GAIN_NAME, ATTR_CURRENT, &gainName))
                    {
                        Log::LogE("Failure getting gain name for index %d (%s)",
                                gainIndex, GetErrorMessage().c_str());
                        return false;
                    }
                    gain.desc = gainName;
                }
                else
                {
                    gain.desc = "<unnamed>";
                }

                gain.desc = std::to_string(gain.value) + ": " + gain.desc;
                gains.push_back(gain);
            }

            speed.port = portItem;
            speed.speedIndex = speedIndex;
            speed.bitDepth = bitDepth;
            speed.pixTimeNs = pixTimeNs;
            speed.gains = gains;
            // Use stream to format double without trailing zeroes
            std::ostringstream ss;
            ss << "P" << speed.port.value << "S" << speed.speedIndex
                << ": " << (double)1000 / speed.pixTimeNs << " MHz, "
                << speed.bitDepth << "b, " << speed.port.desc;
            speed.label = ss.str();
            m_speeds.push_back(speed);
        }
    }

    return true;
}

bool pm::Camera::AllocateBuffers(uns32 frameCount, uns32 frameBytes)
{
    if (m_frameCount == frameCount
            && m_frameAcqCfg.GetFrameBytes() == frameBytes
            && m_buffer)
        return true;

    DeleteBuffers();

    const bool hasMetadata =
        m_settings.GetMetadataCapable() && m_settings.GetMetadataEnabled();
    const bool hasCentroids =
        m_settings.GetCentroidsCapable() && m_settings.GetCentroidsEnabled();

    uint16_t roiCount = (uint16_t)m_settings.GetRegions().size();
    if (hasCentroids)
    {
        roiCount = m_settings.GetCentroidsCount();

        const bool usesCentroidsModeTrack = m_settings.GetCentroidsModeCapable()
            && m_settings.GetCentroidsMode() == PL_CENTROIDS_MODE_TRACK;

        if (usesCentroidsModeTrack)
        {
            roiCount++; // One more ROI for backgroung image sent with particles
        }
    }

    const Frame::AcqCfg frameAcqCfg(frameBytes, roiCount, hasMetadata);

    const uns32 bufferBytes = frameCount * frameBytes;

    if (bufferBytes == 0)
    {
        Log::LogE("Invalid buffer size (0 bytes)");
        return false;
    }

    /*
        HACK: THIS IS VERY DIRTY HACK!!!
        Because of heap corruption that occurs at least with PCIe cameras
        and ROI having position and size with odd numbers, we allocate here
        additional 8 bytes.
        Example rgn_type could be [123,881,1,135,491,1].
        During long investigation I've seen 2, 4 or 6 bytes behind m_buffer
        are always filled with value 0x1c comming probably from PCIe
        driver.
    */
    m_buffer = (void*)new(std::nothrow) uns8[bufferBytes + 8];
    if (!m_buffer)
    {
        Log::LogE("Failure allocating buffer with %u bytes", bufferBytes);
        return false;
    }

    for (uns32 n = 0; n < frameCount; n++)
    {
        std::shared_ptr<pm::Frame> frame;

        try
        {
            // Always shallow copy on top of circular buffer
            frame = std::make_shared<Frame>(frameAcqCfg, false);
        }
        catch (...)
        {
            DeleteBuffers();
            Log::LogE("Failure allocating shallow frame %u copy", n);
            return false;
        }

        // Bind frames to internal buffer
        void* data = (void*)&((uns8*)m_buffer)[n * frameBytes];
        frame->SetDataPointer(data);
        // On shallow copy it does some checks only, not deep copy
        if (!frame->CopyData())
        {
            DeleteBuffers();
            return false;
        }
        frame->OverrideValidity(false); // Force frame to be invalid on start

        m_frames.push_back(frame);
    }
    m_frames.shrink_to_fit();

    m_frameAcqCfg = frameAcqCfg;
    m_frameCount = frameCount;

    return true;
}

void pm::Camera::DeleteBuffers()
{
    m_frames.clear();
    m_framesMap.clear();

    delete [] (uns8*)m_buffer;
    m_buffer = nullptr;

    m_frameAcqCfg = Frame::AcqCfg();
    m_frameCount = 0;
}
