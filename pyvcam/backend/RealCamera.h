#pragma once
#ifndef PM_REAL_CAMERA_H
#define PM_REAL_CAMERA_H

/* System */
#include <atomic>
#include <future>

/* Local */
#include "Camera.h"

namespace pm {

class Frame;

// The class in charge of PVCAM calls
class RealCamera final : public Camera
{
public:
    static void PV_DECL TimeLapseCallbackHandler(FRAME_INFO* frameInfo,
            void* RealCamera_pointer);

public:
    RealCamera();
    virtual ~RealCamera();

    RealCamera(const RealCamera&) = delete;
    RealCamera& operator=(const RealCamera&) = delete;

public: // From Camera
    virtual bool Initialize() override;
    virtual bool Uninitialize() override;
    virtual bool IsInitialized() const override
    { return s_isInitialized; }

    virtual bool GetCameraCount(int16& count) const override;
    virtual bool GetName(int16 index, std::string& name) const override;

    virtual std::string GetErrorMessage() const override;

    virtual bool Open(const std::string& name) override;
    virtual bool Close() override;

    virtual bool SetupExp(const SettingsReader& settings) override;
    virtual bool StartExp(CallbackEx3Fn callbackHandler, void* callbackContext) override;
    virtual bool StopExp() override;
    virtual AcqStatus GetAcqStatus() const override;

    virtual bool SetParam(uns32 id, void* param) override;
    virtual bool GetParam(uns32 id, int16 attr, void* param) const override;
    virtual bool GetEnumParam(uns32 id, std::vector<EnumItem>& items) const override;

    virtual bool GetLatestFrame(Frame& frame) const override;

private:
    // Called from callback function to handle new frame
    void HandleTimeLapseEofCallback(FRAME_INFO* frameInfo);

private:
    static bool s_isInitialized; // PVCAM init state is common for all cameras

private:
    std::atomic<unsigned int> m_timeLapseFrameCount;
    std::future<void> m_timeLapseFuture;

    // Used for an outside entity to receive callbacks from PVCAM
    CallbackEx3Fn m_callbackHandler;
    // Used for an outside entity to receive callbacks from PVCAM
    void* m_callbackContext;

    FRAME_INFO* m_latestFrameInfo;
};

} // namespace pm

#endif /* PM_REAL_CAMERA_H */
