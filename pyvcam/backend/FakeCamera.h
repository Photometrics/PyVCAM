#pragma once
#ifndef PM_FAKE_CAMERA_H
#define PM_FAKE_CAMERA_H

/* System */
#include <atomic>
#include <mutex>

/* Local */
#include "Camera.h"
#include "Timer.h"

namespace std
{
    class thread;
}

namespace pm {

class Frame;

// The class in charge of PVCAM calls
class FakeCamera final : public Camera
{
public:
    explicit FakeCamera(unsigned int targetFps);
    virtual ~FakeCamera();

    FakeCamera() = delete;
    FakeCamera(const FakeCamera&) = delete;
    FakeCamera& operator=(const FakeCamera&) = delete;

public:
    unsigned int GetTargetFps() const;

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

protected: // From Camera
    virtual bool AllocateBuffers(uns32 frameCount, uns32 frameBytes) override;
    virtual void DeleteBuffers() override;

private:
    // Calculate frame size including metadata if any
    uns32 CalculateFrameBytes() const;
    // Draws fake particles into given buffer
    void InjectParticles(uns16* pBuffer, uns16 particleIntensity);
    // Generates Frame header
    md_frame_header GenerateFrameHeader(uns32 frameIndex);

    // Get particle header based on coordinates
    md_frame_roi_header GenerateParticleHeader(uns16 roiIndex, uns16 x, uns16 y);
    // Generate random particles
    void GenerateParticles();
    // Moves each particle in random way
    void MoveParticles();
    // Generated frame data including metadata if any
    void GenerateFrameData();

    // This is the function used to generate frames.
    void FrameGeneratorLoop(); // Routine launched by m_framegenThread

    // Returns the desired size (number of frames) of the circular (or sequence)
    // frame buffer
    uns32 GetDesiredFrameBufferSizeInFrames(const SettingsReader& settings);

private:
    static bool s_isInitialized; // Init state is common for all cameras

private:
    const unsigned int m_targetFps;
    const uns32 m_readoutTimeMs; // Calculated from FPS

    uns16 m_frameRoiExtMdSize;
    std::vector<std::pair<uns16, uns16>> m_particleCoordinates;
    std::vector<std::pair<uns32, uns32>> m_particleMoments;

    // Used for an outside entity to receive callbacks from PVCAM
    CallbackEx3Fn m_callbackHandler;
    // Used for an outside entity to receive callbacks from PVCAM
    void* m_callbackContext;

    Timer m_startStopTimer;

    // Buffer of generated data used for each frame
    void*              m_frameGenBuffer;
    /* Used for artifically constructed buffer to label where we currently are,
       this is the substitution for get_latest_frame. */
    size_t             m_frameGenBufferPos;
    // Used to track frame index of generated frame in whole acquisition
    size_t             m_frameGenFrameIndex;
    // Generated frame info
    FRAME_INFO         m_frameGenFrameInfo;
    // Flag to tell frame generation method when it should stop
    std::atomic<bool>  m_frameGenStopFlag;
    // Actual thread object used to run FrameGenerator
    std::thread*       m_frameGenThread;
    // Mutex that guards all m_frameGen* variables
    mutable std::mutex m_frameGenMutex;
};

} // namespace pm

#endif /* PM_FAKE_CAMERA_H */
