#pragma once
#ifndef PM_ACQUISITION_H
#define PM_ACQUISITION_H

/* System */
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

/* pvcam_helper_track */
#include <pvcam_helper_track.h>

/* Local */
#include "FpsLimiter.h"
#include "Frame.h"
#include "ListStatistics.h"
#include "PrdFileFormat.h"
#include "Timer.h"

namespace std
{
    class thread;
}

// Forward declaration for FRAME_INFO that satisfies compiler (taken from pvcam.h)
struct _TAG_FRAME_INFO;
typedef struct _TAG_FRAME_INFO FRAME_INFO;

namespace pm {

class Camera;

class Acquisition
{
private:
    static void EofCallback(FRAME_INFO* frameInfo, void* Acquisition_pointer);

public:
    explicit Acquisition(std::shared_ptr<Camera> camera);
    ~Acquisition();

    Acquisition() = delete;
    Acquisition(const Acquisition&) = delete;
    Acquisition& operator=(const Acquisition&) = delete;

    // Starts the acquisition
    bool Start(std::shared_ptr<FpsLimiter> fpsLimiter = nullptr);
    // Returns true if acquisition is running, false otherwise
    bool IsRunning() const;
    // Forces correct acquisition interruption
    void RequestAbort(bool abortBufferedFramesProcessing = true);
    /* Blocks until the acquisition completes or reacts to abort request.
       Return true if stopped due to abort request. */
    bool WaitForStop(bool printStats = false);

    // Returns acquisition related statistics
    void GetAcqStats(double& fps, size_t& framesValid, size_t& framesLost,
            size_t& framesMax, size_t& framesCached);
    // Returns storage/processing related statistics
    void GetDiskStats(double& fps, size_t& framesValid, size_t& framesLost,
            size_t& framesMax, size_t& framesCached);

private:
    // Allocates one new frame
    std::unique_ptr<Frame> AllocateNewFrame();
    std::unique_ptr<Frame> GetUnusedFrame();

    // Put frame back to queue with unused frames
    void UnuseFrame(std::unique_ptr<Frame> frame);

    // Called from callback function to handle new frame
    bool HandleEofCallback();

    // Adds a frame to the "to be processed" queue
    void EnqueueFrameToBeProcessed(std::unique_ptr<Frame> frame);

    // Called from AcqThreadLoop to handle new frame
    bool HandleNewFrame(std::unique_ptr<Frame> frame);

    // Returns the specified frame to the "unused" queue and records the lost
    // frame number for later reporting.
    void HandleLostFrame(std::unique_ptr<Frame> frame);

    // Records the specified lost frame number for later reporting
    void HandleLostFrameNumber(const uint32_t frame_number);

    // Adds the specified frame to the "to be saved" queue
    void EnqueueFrameToBeSaved(std::unique_ptr<Frame> frame);

    // Updates max. allowed number of frames in queue to be saved
    size_t UpdateToBeSavedFramesMax();
    // Preallocate or release some ready-to-use frames at start/end
    bool PreallocateUnusedFrames();
    // Configures how frames will be stored on disk
    bool ConfigureStorage();

    // The function performs in m_acqThread, caches frames from camera
    void AcqThreadLoop();
    // The function performs in m_diskThread, saves frames to disk
    void DiskThreadLoop();
    // Called from DiskThreadLoop for one frame per file
    void DiskThreadLoop_Single();
    // Called from DiskThreadLoop for stacked frames in file
    void DiskThreadLoop_Stack();
    // The function performs in m_updateThread, saves frames to disk
    void UpdateThreadLoop();

    void PrintAcqThreadStats() const;
    void PrintDiskThreadStats() const;

    // Returns exposure time for given frame based on configuration (VTM, etc.)
    uint32_t GetFrameExpTime(uint32_t frameNr);

private:
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<FpsLimiter> m_fpsLimiter;

    uint32_t m_maxFramesPerStack; // Limited to 32 bit

    // Uncaught frames statistics
    ListStatistics<size_t> m_uncaughtFrames;
    // Unsaved frames statistics
    ListStatistics<size_t> m_unsavedFrames;

    std::thread*      m_acqThread;
    std::atomic<bool> m_acqThreadAbortFlag;
    std::atomic<bool> m_acqThreadDoneFlag;
    std::thread*      m_diskThread;
    std::atomic<bool> m_diskThreadAbortFlag;
    std::atomic<bool> m_diskThreadDoneFlag;
    std::thread*      m_updateThread;

    // The Timer used for m_acqTime
    Timer m_acqTimer;
    // Time taken to finish acquisition, zero if in progress
    double m_acqTime;
    // The Timer used for m_diskTime
    Timer m_diskTimer;
    // Time taken to finish saving, zero if in progress
    double m_diskTime;

    // Last processed frame number
    uint32_t m_last_processed_frame_number;

    // Latest frame number received from the camera
    std::atomic<uint32_t> m_latest_received_frame_number;

    std::atomic<size_t> m_outOfOrderFrameCount;

    // Mutex that guards all non-atomic m_updateThread* variables
    std::mutex              m_updateThreadMutex;
    // Condition the update thread waits on for new update iteration
    std::condition_variable m_updateThreadCond;

    /*
       Data flow is like this:
       1. In callback handler is:
          - taken one frame from m_unusedFrames queue or allocated new Frame
            instance if queue is empty
          - stored frame info and pointer to data (shallow copy only) in frame
          - frame put to m_toBeProcessedFrames queue
       2. In acquisition thread is:
          - made deep copy of frame's data
          - done checks for lost frames
          - frame moved to m_toBeSavedFrames queue
       3. In disk thread is:
          - frame stored to disk in chosen format
          - frame moved back to m_unusedFrames queue
    */

    // Frames captured in callback thread to be processed in acquisition thread
    std::queue<std::unique_ptr<Frame>>  m_toBeProcessedFrames;
    // Mutex that guards all non-atomic m_toBeProcessed* variables
    std::mutex                          m_toBeProcessedFramesMutex;
    // Condition the acquisition thread waits on for new frame
    std::condition_variable             m_toBeProcessedFramesCond;
    // Current size of queue with captured frames (for stats)
    std::atomic<size_t>                 m_toBeProcessedFramesSize;
    // Maximal size of queue with captured frames
    std::atomic<size_t>                 m_toBeProcessedFramesMax;
    // Highest number of frames that were ever stored in this queue
    std::atomic<size_t>                 m_toBeProcessedFramesMaxPeak;
    // Holds how many new frames have been lost
    std::atomic<size_t>                 m_toBeProcessedFramesLost;
    // Holds how many new frames have been processed
    std::atomic<size_t>                 m_toBeProcessedFramesValid;

    // Frames queued in acquisition thread to be saved to disk
    std::queue<std::unique_ptr<Frame>>  m_toBeSavedFrames;
    // Mutex that guards all non-atomic m_toBeSavedFrames* variables
    std::mutex                          m_toBeSavedFramesMutex;
    // Condition the frame saving thread waits on for new frame
    std::condition_variable             m_toBeSavedFramesCond;
    // Current size of the "to be saved frames" queue (for stats)
    std::atomic<size_t>                 m_toBeSavedFramesSize;
    // Maximal size of the "to be saved frames" queue
    std::atomic<size_t>                 m_toBeSavedFramesMax;
    // Highest number of frames that were ever stored in m_toBeSavedFrames queue
    std::atomic<size_t>                 m_toBeSavedFramesMaxPeak;
    // Holds how many queued frames have not been saved to disk
    std::atomic<size_t>                 m_toBeSavedFramesLost;
    // Holds how many new frames have been processed
    std::atomic<size_t>                 m_toBeSavedFramesValid;
    // Holds how many queued frames have been saved to disk
    std::atomic<size_t>                 m_toBeSavedFramesSaved;

    // Unused but allocated frames to be re-used
    std::queue<std::unique_ptr<Frame>>  m_unusedFrames;
    // Mutex that guards all non-atomic m_unusedFrames* variables
    std::mutex                          m_unusedFramesMutex;
};

} // namespace pm

#endif /* PM_ACQUISITION_H */
