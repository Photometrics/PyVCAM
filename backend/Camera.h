#pragma once
#ifndef PM_CAMERA_H
#define PM_CAMERA_H

/* System */
#include <map>
#include <memory> // std::shared_ptr
#include <string>
#include <vector>

/* PVCAM */
#include <master.h>
#include <pvcam.h>

/* Local */
#include "Frame.h"
#include "Settings.h"

// Function used as an interface between the queue and the callback
using CallbackEx3Fn = void (*)(FRAME_INFO* frameInfo, void* context);

namespace pm {

struct EnumItem
{
    int32 value;
    std::string desc;
};

// The base class for all kind of cameras
class Camera
{
public:
    struct Speed
    {
        EnumItem port; // PARAM_READOUT_PORT
        int16 speedIndex; // PARAM_SPDTAB_INDEX
        uns16 bitDepth; // PARAM_BIT_DEPTH
        uns16 pixTimeNs; // PARAM_PIX_TIME
        std::vector<EnumItem> gains; // PARAM_GAIN_INDEX (+ PARAM_GAIN_NAME)
        std::string label; // Handy to UI labels, e.g. in combo box
    };

    enum class AcqStatus
    {
        Inactive = 0,
        Active,
        Failure
    };

public:
    Camera();
    virtual ~Camera();

    // Library related methods

    // Initialize camera/library
    virtual bool Initialize() = 0;
    // Uninitialize camera/library
    virtual bool Uninitialize() = 0;
    // Current init state
    virtual bool IsInitialized() const = 0;

    // Get number of cameras detected
    virtual bool GetCameraCount(int16& count) const = 0;
    // Get name of the camera on given index
    virtual bool GetName(int16 index, std::string& name) const = 0;

    // Camera related methods

    // Get error message
    virtual std::string GetErrorMessage() const = 0;

    // Open camera, has to be called from derived class upon successfull open
    virtual bool Open(const std::string& name);
    // Close camera, has to be called from derived class upon successfull close
    virtual bool Close();
    // Current open state
    virtual bool IsOpen() const
    { return m_isOpen; }

    // Return camera handle, AKA PVCAM hcam
    virtual int16 GetHandle() const
    { return m_hCam; }

    // Update read-only settings and correct other values that are ussualy valid
    // but are e.g. not supported by this camera. The correction occurs in case
    // user overrided values by custom ones. Otherwise camera-default values are
    // used.
    virtual bool ReviseSettings(Settings& settings);

    // Return settings set via SetupExp
    const SettingsReader& GetSettings() const
    { return m_settings; }

    // Return supported speeds that are obtained on camera open
    const std::vector<Speed>& GetSpeedTable() const
    { return m_speeds; }

    // Setup acquisition
    virtual bool SetupExp(const SettingsReader& settings);
    // Start acquisition
    virtual bool StartExp(CallbackEx3Fn callbackHandler, void* callbackContext) = 0;
    // Stop acquisition
    virtual bool StopExp() = 0;
    // Current acquisition state
    virtual bool IsImaging() const
    { return m_isImaging; }
    // Get acquisition status
    virtual AcqStatus GetAcqStatus() const = 0;

    // Used to generically access the "set_param" api of PVCAM
    virtual bool SetParam(uns32 id, void* param) = 0;
    // Used to generically access the "get_param" api of PVCAM
    virtual bool GetParam(uns32 id, int16 attr, void* param) const = 0;
    // Used to get all enum items via PVCAM api
    virtual bool GetEnumParam(uns32 id, std::vector<EnumItem>& items) const = 0;

    // Get the latest frame and deliver it to the frame being pushed into the queue.
    // It has to call UpdateFrameIndexMap to keep GetFrameIndex work.
    // It has to be called from within EOF callback handler for each frame as
    // there is no other way how to detect the data in raw buffer has changed.
    // The given frame as well as internal frame around raw buffer are invalidated.
    virtual bool GetLatestFrame(Frame& frame) const = 0;
    // Get the frame at index or null (should be used for displaying only)
    virtual std::shared_ptr<Frame> GetFrameAt(size_t index) const;
    // Get index of the frame from circular buffer
    virtual bool GetFrameIndex(const Frame& frame, size_t& index) const;

    // Get current acquisition configuration for frame
    virtual Frame::AcqCfg GetFrameAcqCfg() const
    { return m_frameAcqCfg; }

protected:
    // Updates m_framesMap[oldFrameNr] to m_frame[index].
    // It is const with mutable map to allow usage from GetLatestFrame method.
    virtual void UpdateFrameIndexMap(uint32_t oldFrameNr, size_t index) const;
    // Collects supported speeds
    virtual bool BuildSpeedTable();
    // Allocate internal buffers
    virtual bool AllocateBuffers(uns32 frameCount, uns32 frameBytes);
    // Make sure the buffer is freed and the head pointer is chained at NULL
    virtual void DeleteBuffers();

protected:
    int16 m_hCam;
    bool m_isOpen;
    bool m_isImaging;
    SettingsReader m_settings;
    std::vector<Speed> m_speeds;

    Frame::AcqCfg m_frameAcqCfg; // Number of bytes in one frame in buffer, etc.
    uns32 m_frameCount; // Number of frames in buffer (circ/sequence)
    void* m_buffer; // PVCAM buffer

    std::vector<std::shared_ptr<Frame>> m_frames;
    // Lookup map - frameNr is the key, index to m_frames vector is the value
    mutable std::map<uint32_t, size_t> m_framesMap;
};

} // namespace pm

#endif /* PM_CAMERA_H */
