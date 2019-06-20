#pragma once
#ifndef PM_SETTINGS_H
#define PM_SETTINGS_H

/* System */
#include <string>
#include <vector>

/* PVCAM */
#include <master.h>
#include <pvcam.h>

/* Local */
#include "PrdFileFormat.h"
#include "SettingsReader.h"

namespace pm {

class Settings : public SettingsReader
{
public:
    class ReadOnlyWriter
    {
        friend class Settings;

    private: // Do not allow creation from anywhere else than Settings class
        explicit ReadOnlyWriter(Settings& settings);

        ReadOnlyWriter() = delete;
        ReadOnlyWriter(const ReadOnlyWriter&) = delete;
        ReadOnlyWriter& operator=(const ReadOnlyWriter&) = delete;

    public:
        bool SetEmGainCapable(bool value);
        bool SetEmGainMax(uns16 value);
        bool SetBitDepth(uns16 value);
        bool SetWidth(uns16 value);
        bool SetHeight(uns16 value);
        bool SetCircBufferCapable(bool value);
        bool SetMetadataCapable(bool value);
        bool SetColorMask(int32 value);
        bool SetRegionCountMax(uns16 value);
        bool SetCentroidsCapable(bool value);
        bool SetCentroidsModeCapable(bool value);
        bool SetCentroidsCountMax(uns16 value);
        bool SetCentroidsRadiusMax(uns16 value);
        bool SetCentroidsBgCountCapable(bool value);
        bool SetCentroidsThresholdCapable(bool value);

    private:
        Settings& m_settings;
    };

public:
    Settings();
    virtual ~Settings();

public:
    ReadOnlyWriter& GetReadOnlyWriter();

public: // To be called by anyone
    bool SetCamIndex(int16 value);

    bool SetPortIndex(int32 value);
    bool SetSpeedIndex(int16 value);
    bool SetGainIndex(int16 value);

    bool SetEmGain(uns16 value);
    bool SetClrCycles(uns16 value);
    bool SetClrMode(int32 value);
    bool SetPMode(int32 value);
    bool SetTrigMode(int32 value);
    bool SetExpOutMode(int32 value);
    bool SetMetadataEnabled(bool value);
    bool SetTrigTabSignal(int32 value);
    bool SetLastMuxedSignal(uns8 value);
    bool SetExposureResolution(int32 value);

    bool SetAcqFrameCount(uns32 value);
    bool SetBufferFrameCount(uns32 value);

    bool SetBinningSerial(uns16 value);
    bool SetBinningParallel(uns16 value);
    bool SetRegions(const std::vector<rgn_type>& value);

    bool SetExposure(uns32 value);
    bool SetVtmExposures(const std::vector<uns16>& value);
    bool SetAcqMode(AcqMode value);
    bool SetTimeLapseDelay(unsigned int value);

    bool SetStorageType(StorageType value);
    bool SetSaveDir(const std::string& value);
    bool SetSaveFirst(size_t value);
    bool SetSaveLast(size_t value);
    bool SetMaxStackSize(size_t value);

    bool SetCentroidsEnabled(bool value);
    bool SetCentroidsCount(uns16 value);
    bool SetCentroidsRadius(uns16 value);
    bool SetCentroidsMode(int32 value);
    bool SetCentroidsBackgroundCount(int32 value);
    bool SetCentroidsThreshold(uns32 value);

    bool SetTrackLinkFrames(uns16 value);
    bool SetTrackMaxDistance(uns16 value);
    bool SetTrackCpuOnly(bool value);
    bool SetTrackTrajectoryDuration(uns16 value);

protected:
    ReadOnlyWriter m_readOnlyWriter;
};

} // namespace pm

#endif /* PM_SETTINGS_H */
