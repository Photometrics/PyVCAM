#include "Acquisition.h"

/* System */
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>

/* Local */
#include "Camera.h"
#include "FakeCamera.h"
#include "Log.h"
#include "PrdFileSave.h"
#include "PrdFileUtils.h"
#include "TiffFileSave.h"
#include "Utils.h"

void pm::Acquisition::EofCallback(FRAME_INFO* frameInfo, void* Acquisition_pointer)
{
    Acquisition* acq = static_cast<Acquisition*>(Acquisition_pointer);
    if (!frameInfo)
    {
        acq->RequestAbort();
    }
    if (!acq->HandleEofCallback())
    {
        acq->RequestAbort(false); // Let queued frames to be processed
    }
}

pm::Acquisition::Acquisition(std::shared_ptr<Camera> camera)
    : m_camera(camera),
    m_fpsLimiter(nullptr),
    m_maxFramesPerStack(0),
    m_uncaughtFrames(),
    m_unsavedFrames(),
    m_acqThread(nullptr),
    m_acqThreadAbortFlag(false),
    m_acqThreadDoneFlag(false),
    m_diskThread(nullptr),
    m_diskThreadAbortFlag(false),
    m_diskThreadDoneFlag(false),
    m_updateThread(nullptr),
    m_acqTimer(),
    m_acqTime(0.0),
    m_diskTimer(),
    m_diskTime(0.0),
    m_lastFrameNumber(0),
    m_outOfOrderFrameCount(0),
    m_updateThreadMutex(),
    m_updateThreadCond(),
    m_toBeProcessedFrames(),
    m_toBeProcessedFramesMutex(),
    m_toBeProcessedFramesCond(),
    m_toBeProcessedFramesSize(0),
    m_toBeProcessedFramesMax(1),
    m_toBeProcessedFramesMaxPeak(0),
    m_toBeProcessedFramesLost(0),
    m_toBeProcessedFramesValid(0),
    m_toBeSavedFrames(),
    m_toBeSavedFramesMutex(),
    m_toBeSavedFramesCond(),
    m_toBeSavedFramesSize(0),
    m_toBeSavedFramesMax(1),
    m_toBeSavedFramesMaxPeak(0),
    m_toBeSavedFramesLost(0),
    m_toBeSavedFramesValid(0),
    m_toBeSavedFramesSaved(0),
    m_unusedFrames(),
    m_unusedFramesMutex()
{
}

pm::Acquisition::~Acquisition()
{
    RequestAbort();
    WaitForStop();

    // No need to lock queues below, they are private and all threads are stopped

    // Free unused frames
    while (!m_toBeProcessedFrames.empty())
    {
        m_toBeProcessedFrames.front().reset();
        m_toBeProcessedFrames.pop();
    }

    // Free queued but not processed frames
    while (!m_toBeSavedFrames.empty())
    {
        m_toBeSavedFrames.front().reset();
        m_toBeSavedFrames.pop();
    }

    // Free unused frames
    while (!m_unusedFrames.empty())
    {
        m_unusedFrames.front().reset();
        m_unusedFrames.pop();
    }
}

bool pm::Acquisition::Start(std::shared_ptr<FpsLimiter> fpsLimiter)
{
    if (!m_camera)
        return false;

    if (IsRunning())
        return true;

    /* The option below is used for testing purposes, but also for
        demonstration in terms of what places in the code would need to be
        altered to NOT save frames to disk */
    if (!ConfigureStorage())
        return false;

    if (!PreallocateUnusedFrames())
        return false;

    m_fpsLimiter = fpsLimiter;

    m_acqThreadAbortFlag = false;
    m_acqThreadDoneFlag = false;
    m_diskThreadAbortFlag = false;
    m_diskThreadDoneFlag = false;

    /* Start all threads but acquisition first to reduce the overall system
       load after starting the acquisition */
    m_diskThread =
        new(std::nothrow) std::thread(&Acquisition::DiskThreadLoop, this);
    if (m_diskThread)
    {
        m_updateThread =
            new(std::nothrow) std::thread(&Acquisition::UpdateThreadLoop, this);
        if (m_updateThread)
        {
            m_acqThread =
                new(std::nothrow) std::thread(&Acquisition::AcqThreadLoop, this);
        }

        if (!m_updateThread || !m_acqThread)
        {
            RequestAbort();
            WaitForStop(); // Returns true - aborted
        }
    }

    return IsRunning();
}

bool pm::Acquisition::IsRunning() const
{
    return (m_acqThread || m_diskThread || m_updateThread);
}

void pm::Acquisition::RequestAbort(bool abortBufferedFramesProcessing)
{
    m_acqThreadAbortFlag = true;
    if (m_acqThread)
    {
        // Wake acq waiter
        m_toBeProcessedFramesCond.notify_all();
    }
    else
    {
        m_acqThreadDoneFlag = true;
    }

    if (abortBufferedFramesProcessing)
    {
        m_diskThreadAbortFlag = true;
        if (m_diskThread)
        {
            // Wake disk waiter
            m_toBeSavedFramesCond.notify_all();
        }
        else
        {
            m_diskThreadDoneFlag = true;

            // Wake update thread, just in case
            m_updateThreadCond.notify_all();
        }
    }
}

bool pm::Acquisition::WaitForStop(bool printStats)
{
    const bool printEndMessage = m_acqThread && m_diskThread && m_updateThread;

    if (m_acqThread)
    {
        if (m_acqThread->joinable())
            m_acqThread->join();
        delete m_acqThread;
        m_acqThread = nullptr;
    }
    if (m_diskThread)
    {
        if (m_diskThread->joinable())
            m_diskThread->join();
        delete m_diskThread;
        m_diskThread = nullptr;
    }
    if (m_updateThread)
    {
        if (m_updateThread->joinable())
            m_updateThread->join();
        delete m_updateThread;
        m_updateThread = nullptr;
    }

    if (printStats)
    {
        PrintAcqThreadStats();
        PrintDiskThreadStats();
    }

    const bool wasAborted = m_acqThreadAbortFlag || m_diskThreadAbortFlag;

    if (printEndMessage)
    {
        if (wasAborted)
            Log::LogI("Acquisition stopped\n");
        else
            Log::LogI("Acquisition finished\n");
    }

    // After full stop release most of the frames to free RAM. It is done
    // anyway at next acq start. Frame cfg is unchanged so it cannot fail.
    PreallocateUnusedFrames();

    return wasAborted;
}

void pm::Acquisition::GetAcqStats(double& fps, size_t& framesValid,
        size_t& framesLost, size_t& framesMax, size_t& framesCached)
{
    framesValid = m_toBeProcessedFramesValid;
    framesLost = m_toBeProcessedFramesLost;
    framesMax = std::max<size_t>(1, m_toBeProcessedFramesMax);
    framesCached = std::max<size_t>(1, m_toBeProcessedFramesSize);
    const double time = m_acqTimer.Seconds();
    fps = (!m_acqThread || m_acqThreadDoneFlag || time <= 0.0)
        ? 0.0
        : (framesValid + framesLost) / time;
}

void pm::Acquisition::GetDiskStats(double& fps, size_t& framesValid,
        size_t& framesLost, size_t& framesMax, size_t& framesCached)
{
    framesValid = m_toBeSavedFramesValid;
    framesLost = m_toBeSavedFramesLost;
    framesMax = std::max<size_t>(1, m_toBeSavedFramesMax);
    framesCached = std::max<size_t>(1, m_toBeSavedFramesSize);
    const double time = m_acqTimer.Seconds();
    fps = (!m_diskThread || m_diskThreadDoneFlag || time <= 0.0)
        ? 0.0
        : (framesValid + framesLost) / time;
}

std::unique_ptr<pm::Frame> pm::Acquisition::AllocateNewFrame()
{
    /* We use references when in sequence mode because the frame data isn't
       going anywhere so there's no reason to copy it out of PVCAM's buffer */

    return std::make_unique<Frame>(m_camera->GetFrameAcqCfg(),
            m_camera->GetSettings().GetAcqMode() != AcqMode::SnapSequence);
}

void pm::Acquisition::UnuseFrame(std::unique_ptr<Frame> frame)
{
    std::unique_lock<std::mutex> lock(m_unusedFramesMutex);
    m_unusedFrames.push(std::move(frame));
}

bool pm::Acquisition::HandleEofCallback()
{
    if (m_acqThreadAbortFlag)
        return true; // Return value doesn't matter, abort is already in progress

    std::unique_ptr<Frame> frame = nullptr;
    {
        std::unique_lock<std::mutex> lock(m_unusedFramesMutex);
        // Get free frame buffer, either create new or re-use old one
        if (m_toBeProcessedFramesSize < m_toBeProcessedFramesMax)
        {
            if (!m_unusedFrames.empty())
            {
                frame = std::move(m_unusedFrames.front());
                m_unusedFrames.pop();
            }
            else
            {
                frame = std::move(AllocateNewFrame());
            }
        }
    }
    if (!frame)
        // No room for new frame, dropping it and keep going
        return true;

    if (!m_camera->GetLatestFrame(*frame))
    {
        // Abort, could happen e.g. if frame number is 0
        UnuseFrame(std::move(frame));
        return false;
    }

    // Put frame to queue for processing
    {
        std::unique_lock<std::mutex> lock(m_toBeProcessedFramesMutex);

        m_toBeProcessedFrames.push(std::move(frame));
        m_toBeProcessedFramesSize = m_toBeProcessedFrames.size();

        if (m_toBeProcessedFramesMaxPeak < m_toBeProcessedFramesSize)
        {
            // operator= cannot be used as it is deleted
            m_toBeProcessedFramesMaxPeak.exchange(m_toBeProcessedFramesSize);
        }
    }
    // Notify all waiters about new captured frame
    m_toBeProcessedFramesCond.notify_all();

    return true;
}

bool pm::Acquisition::HandleNewFrame(std::unique_ptr<Frame> frame)
{
    // Do deep copy
    if (!frame->CopyData())
        return false;

    // Sync validity of frame in camera's circular buffer
    size_t index;
    if (m_camera->GetFrameIndex(*frame, index))
    {
        auto camFrame = m_camera->GetFrameAt(index);
        if (camFrame)
        {
            camFrame->OverrideValidity(frame->IsValid());
        }
    }

    const uint32_t frameNr = frame->GetInfo().GetFrameNr();

    if (frameNr <= m_lastFrameNumber)
    {
        m_outOfOrderFrameCount++;

        Log::LogE("Frame number out of order: %u, last frame number was %u, ignoring",
                frameNr, m_lastFrameNumber);

        m_toBeProcessedFramesLost++;
        // Drop frame for invalid frame number
        UnuseFrame(std::move(frame));

        // Number out of order, cannot add it to m_unsavedFrames stats
        return true;
    }

    // Check to make sure we didn't skip a frame
    const uint32_t lostFrameCount = frameNr - m_lastFrameNumber - 1;
    if (lostFrameCount > 0)
    {
        m_toBeProcessedFramesLost += lostFrameCount;

        // Log all the frame numbers we missed
        for (uint32_t frameNumber = m_lastFrameNumber + 1;
                frameNumber < frameNr; frameNumber++)
        {
            // TODO: Handle return value, abort on failure?
            m_uncaughtFrames.AddItem(frameNumber);
        }
    }
    m_lastFrameNumber = frameNr;

    m_toBeProcessedFramesValid++;

    // Send frame to GUI here so display is not slowed down by saving images.
    if (m_fpsLimiter)
    {
        // Pass frame copy to FPS limiter for later processing in GUI
        m_fpsLimiter->InputNewFrame(frame->Clone());
    }

    // Context for saveLock to be held
    {
        std::unique_lock<std::mutex> saveLock(m_toBeSavedFramesMutex);

        if (m_toBeSavedFramesSize < m_toBeSavedFramesMax)
        {
            m_toBeSavedFrames.push(std::move(frame));
            m_toBeSavedFramesSize = m_toBeSavedFrames.size();

            if (m_toBeSavedFramesMaxPeak < m_toBeSavedFramesSize)
            {
                // operator= cannot be used as it is deleted
                m_toBeSavedFramesMaxPeak.exchange(m_toBeSavedFramesSize);
            }
        }
        else
        {
            m_toBeSavedFramesLost++;

            saveLock.unlock();

            // Drop the frame, not enough RAM to queue it for saving
            UnuseFrame(std::move(frame));

            // Log the dropped frame
            m_unsavedFrames.AddItem(
                    m_toBeProcessedFramesValid + m_toBeProcessedFramesLost);
        }
    }
    // Notify all waiters about new queued frame
    m_toBeSavedFramesCond.notify_all();

    return true;
}

void pm::Acquisition::UpdateToBeSavedFramesMax()
{
    static const size_t totalRamMB = GetTotalRamMB();
    const size_t availRamMB = GetAvailRamMB();
    /* We allow allocation of memory up to bigger value from these:
       - 90% of total RAM
       - whole available RAM reduced by 1024MB */
    const size_t dontTouchRamMB = std::min<size_t>(totalRamMB * (100 - 90) / 100, 1024);
    const size_t maxFreeRamMB =
        (availRamMB >= dontTouchRamMB) ? availRamMB - dontTouchRamMB : 0;
    // Left shift by 20 bits "converts" megabytes to bytes
    const size_t maxFreeRamBytes = maxFreeRamMB << 20;

    const size_t frameBytes = m_camera->GetFrameAcqCfg().GetFrameBytes();
    const size_t maxNewFrameCount =
        (frameBytes == 0) ? 0 : maxFreeRamBytes / frameBytes;

    m_toBeSavedFramesMax = m_toBeSavedFramesSize + maxNewFrameCount;
}

bool pm::Acquisition::PreallocateUnusedFrames()
{
    // Limit the queue with captured frames to half of the circular buffer size
    m_toBeProcessedFramesMax =
        (m_camera->GetSettings().GetBufferFrameCount() / 2) + 1;

    UpdateToBeSavedFramesMax();

    const Frame::AcqCfg frameAcqCfg = m_camera->GetFrameAcqCfg();
    const size_t frameCount = m_camera->GetSettings().GetAcqFrameCount();
    const size_t frameBytes = frameAcqCfg.GetFrameBytes();
    const size_t frameCountIn100MB =
        (frameBytes == 0) ? 0 : ((100 << 20) / frameBytes);
    const size_t recommendedFrameCount = std::min<size_t>(
            10 + std::min<size_t>(frameCount, frameCountIn100MB),
            m_toBeSavedFramesMax);

    // Moved unprocessed frames to unused frames queue
    while (!m_toBeProcessedFrames.empty())
    {
        std::unique_ptr<Frame> frame = std::move(m_toBeProcessedFrames.front());
        m_toBeProcessedFrames.pop();
        m_unusedFrames.push(std::move(frame));
    }
    m_toBeProcessedFramesSize = 0;
    // Moved unsaved frames to unused frames queue
    while (!m_toBeSavedFrames.empty())
    {
        std::unique_ptr<Frame> frame = std::move(m_toBeSavedFrames.front());
        m_toBeSavedFrames.pop();
        m_unusedFrames.push(std::move(frame));
    }
    m_toBeSavedFramesSize = 0;

    // Same condition as in AllocateNewFrame method
    const bool deepCopy =
        m_camera->GetSettings().GetAcqMode() != AcqMode::SnapSequence;

    if (!m_unusedFrames.empty()
            && (m_unusedFrames.front()->GetAcqCfg() != frameAcqCfg
                || m_unusedFrames.front()->UsesDeepCopy() != deepCopy))
    {
        // Release all frames, frame has changed
        while (!m_unusedFrames.empty())
        {
            m_unusedFrames.front().reset();
            m_unusedFrames.pop();
        }
    }
    else
    {
        // Release surplus frames
        while (m_unusedFrames.size() > recommendedFrameCount)
        {
            m_unusedFrames.front().reset();
            m_unusedFrames.pop();
        }
    }

    // Allocate some ready-to-use frames
    while (m_unusedFrames.size() < recommendedFrameCount)
    {
        std::unique_ptr<Frame> frame = std::move(AllocateNewFrame());
        if (!frame)
            return false;
        m_unusedFrames.push(std::move(frame));
    }

    return true;
}

bool pm::Acquisition::ConfigureStorage()
{
    const size_t maxStackSize = m_camera->GetSettings().GetMaxStackSize();

    const rgn_type rgn = SettingsReader::GetImpliedRegion(
            m_camera->GetSettings().GetRegions());
    PrdHeader prdHeader;
    ClearPrdHeaderStructure(prdHeader);
    prdHeader.version = PRD_VERSION_0_5;
    prdHeader.bitDepth = m_camera->GetSettings().GetBitDepth();
    memcpy(&prdHeader.region, &rgn, sizeof(PrdRegion));
    prdHeader.sizeOfPrdMetaDataStruct = sizeof(PrdMetaData);
    switch (m_camera->GetSettings().GetExposureResolution())
    {
    case EXP_RES_ONE_MICROSEC:
        prdHeader.exposureResolution = PRD_EXP_RES_US;
        break;
    case EXP_RES_ONE_MILLISEC:
    default: // Just in case, should never happen
        prdHeader.exposureResolution = PRD_EXP_RES_MS;
        break;
    case EXP_RES_ONE_SEC:
        prdHeader.exposureResolution = PRD_EXP_RES_S;
        break;
    }
    prdHeader.colorMask = (uint8_t)m_camera->GetSettings().GetColorMask(); // Since v0.3
    prdHeader.flags = (m_camera->GetFrameAcqCfg().HasMetadata())
        ? PRD_FLAG_HAS_METADATA : 0x00; // Since v0.3
    prdHeader.frameSize = (uint32_t)m_camera->GetFrameAcqCfg().GetFrameBytes(); // Since v0.3

    prdHeader.frameCount = 1;
    const size_t prdSingleBytes = GetPrdFileSizeInBytes(prdHeader);
    Log::LogI("Size of PRD file with single frame: %llu bytes",
            (unsigned long long)prdSingleBytes);

    m_maxFramesPerStack = GetFrameCountThatFitsIn(prdHeader, maxStackSize);

    if (maxStackSize > 0)
    {
        prdHeader.frameCount = m_maxFramesPerStack;
        const size_t prdStackBytes = GetPrdFileSizeInBytes(prdHeader);
        Log::LogI("Max. size of PRD file with up to %u stacked frames: %llu bytes",
                m_maxFramesPerStack, (unsigned long long)prdStackBytes);

        if (m_maxFramesPerStack < 2)
        {
            Log::LogE("Stack size is too small");
            return false;
        }
    }

    UpdateToBeSavedFramesMax();

    return true;
}

void pm::Acquisition::AcqThreadLoop()
{
    m_acqTime = 0.0;

    m_toBeProcessedFramesValid = 0;
    m_toBeProcessedFramesLost = 0;
    m_toBeProcessedFramesMaxPeak = 0;

    m_lastFrameNumber = 0;
    m_outOfOrderFrameCount = 0;
    m_uncaughtFrames.Clear();

    const AcqMode acqMode = m_camera->GetSettings().GetAcqMode();
    const bool isAcqModeLive =
        acqMode == AcqMode::LiveCircBuffer || acqMode == AcqMode::LiveTimeLapse;

    const size_t frameCount = (isAcqModeLive)
        ? 0
        : m_camera->GetSettings().GetAcqFrameCount();

    if (!m_camera->StartExp(&Acquisition::EofCallback, this))
    {
        RequestAbort();
    }
    else
    {
        m_acqTimer.Reset(); // Start up might take some time, ignore it

        Log::LogI("Acquisition has started successfully\n");

        while ((isAcqModeLive
                    || m_toBeProcessedFramesValid + m_toBeProcessedFramesLost < frameCount)
                && !m_acqThreadAbortFlag)
        {
            std::unique_ptr<Frame> frame = nullptr;
            {
                std::unique_lock<std::mutex> lock(m_toBeProcessedFramesMutex);

                if (m_toBeProcessedFrames.empty())
                {
                    const bool timedOut = !m_toBeProcessedFramesCond.wait_for(
                            lock, std::chrono::milliseconds(5000), [this]() {
                                return (!m_toBeProcessedFrames.empty() || m_acqThreadAbortFlag);
                            });
                    if (timedOut)
                    {
                        if (m_camera->GetAcqStatus() == Camera::AcqStatus::Active)
                            continue;

                        Log::LogE("Acquisition seems to be not active anymore");
                        RequestAbort(false); // Let queued frames to be processed
                        break;
                    }
                }
                if (m_acqThreadAbortFlag)
                    break;

                frame = std::move(m_toBeProcessedFrames.front());
                m_toBeProcessedFrames.pop();
                m_toBeProcessedFramesSize = m_toBeProcessedFrames.size();
            }
            // frame is always valid here
            if (!HandleNewFrame(std::move(frame)))
            {
                RequestAbort(false); // Let queued frames to be processed
                break;
            }
        }

        m_acqTime = m_acqTimer.Seconds();

        m_camera->StopExp();

        std::ostringstream ss;
        ss << (m_toBeProcessedFramesValid + m_toBeProcessedFramesLost)
            << " frames acquired from the camera and "
            << m_toBeProcessedFramesValid
            << " of them queued for processing in " << m_acqTime << " seconds";
        Log::LogI(ss.str());
    }

    m_acqThreadDoneFlag = true;

    // Wake disk waiter just in case it will abort right away
    m_toBeSavedFramesCond.notify_all();

    // Allow update thread to finish
    m_updateThreadCond.notify_all();
}

void pm::Acquisition::DiskThreadLoop()
{
    m_diskTimer.Reset();
    m_diskTime = 0.0;

    m_toBeSavedFramesValid = 0;
    m_toBeSavedFramesLost = 0;
    m_toBeSavedFramesMaxPeak = 0;
    m_toBeSavedFramesSaved = 0;
    m_unsavedFrames.Clear();

    const StorageType storageType = m_camera->GetSettings().GetStorageType();
    const size_t maxStackSize = m_camera->GetSettings().GetMaxStackSize();

    if (maxStackSize > 0)
        DiskThreadLoop_Stack();
    else
        DiskThreadLoop_Single();

    m_diskTime = m_diskTimer.Seconds();
    m_diskThreadDoneFlag = true;

    // Allow update thread to finish
    m_updateThreadCond.notify_all();

    // Wait for updateThread thread stop
    m_updateThread->join();

    if (m_diskTime > 0.0)
    {
        std::ostringstream ss;
        ss << m_toBeProcessedFramesValid << " queued frames processed and ";
        switch (storageType)
        {
        case StorageType::Prd:
            ss << m_toBeSavedFramesSaved << " of them saved to PRD file(s)";
            break;
        case StorageType::Tiff:
            ss << m_toBeSavedFramesSaved << " of them saved to TIFF file(s)";
            break;
        case StorageType::None:
            ss << "none of them saved";
            break;
        // No default section, compiler will complain when new format added
        }
        ss << " in " << m_diskTime << " seconds\n";
        Log::LogI(ss.str());
    }
}

void pm::Acquisition::DiskThreadLoop_Single()
{
    const AcqMode acqMode = m_camera->GetSettings().GetAcqMode();
    const bool isAcqModeLive =
        acqMode == AcqMode::LiveCircBuffer || acqMode == AcqMode::LiveTimeLapse;

    const size_t frameCount = (isAcqModeLive)
        ? 0
        : m_camera->GetSettings().GetAcqFrameCount();
    const StorageType storageType = m_camera->GetSettings().GetStorageType();
    const std::string saveDir = m_camera->GetSettings().GetSaveDir();
    const size_t saveFirst = (isAcqModeLive)
        ? m_camera->GetSettings().GetSaveFirst()
        : std::min(frameCount, m_camera->GetSettings().GetSaveFirst());
    const size_t saveLast = (isAcqModeLive)
        ? 0
        : std::min(frameCount, m_camera->GetSettings().GetSaveLast());

    const rgn_type rgn = SettingsReader::GetImpliedRegion(
            m_camera->GetSettings().GetRegions());
    PrdHeader prdHeader;
    ClearPrdHeaderStructure(prdHeader);
    prdHeader.version = PRD_VERSION_0_5;
    prdHeader.bitDepth = m_camera->GetSettings().GetBitDepth();
    memcpy(&prdHeader.region, &rgn, sizeof(PrdRegion));
    prdHeader.sizeOfPrdMetaDataStruct = sizeof(PrdMetaData);
    switch (m_camera->GetSettings().GetExposureResolution())
    {
    case EXP_RES_ONE_MICROSEC:
        prdHeader.exposureResolution = PRD_EXP_RES_US;
        break;
    case EXP_RES_ONE_MILLISEC:
    default: // Just in case, should never happen
        prdHeader.exposureResolution = PRD_EXP_RES_MS;
        break;
    case EXP_RES_ONE_SEC:
        prdHeader.exposureResolution = PRD_EXP_RES_S;
        break;
    }
    prdHeader.frameCount = 1;
    prdHeader.colorMask = (uint8_t)m_camera->GetSettings().GetColorMask(); // Since v0.3
    prdHeader.flags = (m_camera->GetFrameAcqCfg().HasMetadata())
        ? PRD_FLAG_HAS_METADATA : 0x00; // Since v0.3
    prdHeader.frameSize = (uint32_t)m_camera->GetFrameAcqCfg().GetFrameBytes(); // Since v0.3

    // Absolute frame index in saving sequence
    size_t frameIndex = 0;

    while ((isAcqModeLive || frameIndex < frameCount)
            && !m_diskThreadAbortFlag)
    {
        std::unique_ptr<Frame> frame = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_toBeSavedFramesMutex);

            if (m_toBeSavedFrames.empty())
            {
                // There are no queued frames and acquisition has finished, stop this thread
                if (m_acqThreadDoneFlag)
                    break;

                m_toBeSavedFramesCond.wait(lock, [this]() {
                    const bool empty = m_toBeSavedFrames.empty();
                    return (!empty || m_diskThreadAbortFlag
                            || (m_acqThreadDoneFlag && empty));
                });
            }
            if (m_diskThreadAbortFlag)
                break;
            if (m_acqThreadDoneFlag && m_toBeSavedFrames.empty())
                break;

            frame = std::move(m_toBeSavedFrames.front());
            m_toBeSavedFrames.pop();
            m_toBeSavedFramesSize = m_toBeSavedFrames.size();
        }

        bool keepGoing = true;

        // Frame is sent to GUI in acquisition thread
        if (m_acqThreadDoneFlag && m_fpsLimiter)
        {
            // Pass null frame to FPS limiter for later processing in GUI
            // to let GUI know that disk thread is still working
            m_fpsLimiter->InputNewFrame(nullptr);
        }
        m_toBeSavedFramesValid++;

        const bool doSaveFirst = saveFirst > 0 && frameIndex < saveFirst;
        const bool doSaveLast = saveLast > 0 && frameIndex >= frameCount - saveLast;
        const bool doSaveAll = (saveFirst == 0 && saveLast == 0)
            || (!isAcqModeLive && saveFirst >= frameCount - saveLast);
        const bool doSave = doSaveFirst || doSaveLast || doSaveAll;

        if (storageType != StorageType::None && doSave)
        {
            // Used frame number instead of frameIndex
            std::string fileName;
            fileName = ((saveDir.empty()) ? "." : saveDir) + "/";
            fileName += "ss_single_"
                + std::to_string(frame->GetInfo().GetFrameNr());
            FileSave* file = nullptr;

            switch (storageType)
            {
            case StorageType::Prd:
                fileName += ".prd";
                file = new(std::nothrow) PrdFileSave(fileName, prdHeader);
                break;
            case StorageType::Tiff:
                fileName += ".tiff";
                file = new(std::nothrow) TiffFileSave(fileName, prdHeader);
                break;
            case StorageType::None:
                break;
            // No default section, compiler will complain when new format added
            }

            if (!file || !file->Open())
            {
                Log::LogE("Error in writing data at %s", fileName.c_str());
                keepGoing = false;
            }
            else
            {
                const Frame::Info& fi = frame->GetInfo();
                if (!file->WriteFrame(*frame, GetFrameExpTime(fi.GetFrameNr())))
                {
                    Log::LogE("Error in writing RAW data at %s",
                            fileName.c_str());
                    keepGoing = false;
                }
                else
                {
                    m_toBeSavedFramesSaved++;
                }

                file->Close();
                delete file;
            }
        }

        if (!keepGoing)
            RequestAbort();

        UnuseFrame(std::move(frame));

        frameIndex++;
    }
}

void pm::Acquisition::DiskThreadLoop_Stack()
{
    /* Logic is very similar to code in DiskThreadLoop_Single method but
       the index arithmetics is much more complicated. */

    const AcqMode acqMode = m_camera->GetSettings().GetAcqMode();
    const bool isAcqModeLive =
        acqMode == AcqMode::LiveCircBuffer || acqMode == AcqMode::LiveTimeLapse;

    const size_t frameCount = (isAcqModeLive)
        ? 0
        : m_camera->GetSettings().GetAcqFrameCount();
    const StorageType storageType = m_camera->GetSettings().GetStorageType();
    const std::string saveDir = m_camera->GetSettings().GetSaveDir();
    const size_t saveFirst = (isAcqModeLive)
        ? m_camera->GetSettings().GetSaveFirst()
        : std::min(frameCount, m_camera->GetSettings().GetSaveFirst());
    const size_t saveLast = (isAcqModeLive)
        ? 0
        : std::min(frameCount, m_camera->GetSettings().GetSaveLast());
    const size_t maxStackSize = m_camera->GetSettings().GetMaxStackSize();

    const rgn_type rgn = SettingsReader::GetImpliedRegion(
            m_camera->GetSettings().GetRegions());
    PrdHeader prdHeader;
    ClearPrdHeaderStructure(prdHeader);
    prdHeader.version = PRD_VERSION_0_5;
    prdHeader.bitDepth = m_camera->GetSettings().GetBitDepth();
    memcpy(&prdHeader.region, &rgn, sizeof(PrdRegion));
    prdHeader.sizeOfPrdMetaDataStruct = sizeof(PrdMetaData);
    switch (m_camera->GetSettings().GetExposureResolution())
    {
    case EXP_RES_ONE_MICROSEC:
        prdHeader.exposureResolution = PRD_EXP_RES_US;
        break;
    case EXP_RES_ONE_MILLISEC:
    default: // Just in case, should never happen
        prdHeader.exposureResolution = PRD_EXP_RES_MS;
        break;
    case EXP_RES_ONE_SEC:
        prdHeader.exposureResolution = PRD_EXP_RES_S;
        break;
    }
    //prdHeader.frameCount calculated later
    prdHeader.colorMask = (uint8_t)m_camera->GetSettings().GetColorMask(); // Since v0.3
    prdHeader.flags = (m_camera->GetFrameAcqCfg().HasMetadata())
        ? PRD_FLAG_HAS_METADATA : 0x00; // Since v0.3
    prdHeader.frameSize = (uint32_t)m_camera->GetFrameAcqCfg().GetFrameBytes(); // Since v0.3

    const uint32_t maxFramesPerStack = GetFrameCountThatFitsIn(prdHeader, maxStackSize);

    std::string fileName;
    FileSave* file = nullptr;

    // Absolute frame index in saving sequence
    size_t frameIndex = 0;

    while ((isAcqModeLive || frameIndex < frameCount)
            && !m_diskThreadAbortFlag)
    {
        std::unique_ptr<Frame> frame = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_toBeSavedFramesMutex);

            if (m_toBeSavedFrames.empty())
            {
                // There are no queued frames and acquisition has finished, stop this thread
                if (m_acqThreadDoneFlag)
                    break;

                m_toBeSavedFramesCond.wait(lock, [this]() {
                    const bool empty = m_toBeSavedFrames.empty();
                    return (!empty || m_diskThreadAbortFlag
                            || (m_acqThreadDoneFlag && empty));
                });
            }
            if (m_diskThreadAbortFlag)
                break;
            if (m_acqThreadDoneFlag && m_toBeSavedFrames.empty())
                break;

            frame = std::move(m_toBeSavedFrames.front());
            m_toBeSavedFrames.pop();
            m_toBeSavedFramesSize = m_toBeSavedFrames.size();
        }

        bool keepGoing = true;

        // Frame is sent to GUI in acquisition thread
        if (m_acqThreadDoneFlag && m_fpsLimiter)
        {
            // Pass null frame to FPS limiter for later processing in GUI
            // to let GUI know that disk thread is still working
            m_fpsLimiter->InputNewFrame(nullptr);
        }
        m_toBeSavedFramesValid++;

        const bool doSaveFirst = saveFirst > 0 && frameIndex < saveFirst;
        const bool doSaveLast = saveLast > 0 && frameIndex >= frameCount - saveLast;
        const bool doSaveAll = (saveFirst == 0 && saveLast == 0)
            || (!isAcqModeLive && saveFirst >= frameCount - saveLast);
        const bool doSave = doSaveFirst || doSaveLast || doSaveAll;

        if (storageType != StorageType::None && doSave)
        {
            if (maxFramesPerStack == 0)
            {
                Log::LogE("Unsupported number of frames in stack");
                RequestAbort();
                return;
            }

            /* Index for output file, relative either to sequence beginning
                or to first frame for --save-last option */
            size_t stackIndex;
            // Relative frame index in file, first in file is 0
            size_t frameIndexInStack;
            if (doSaveFirst || doSaveAll)
            {
                stackIndex = frameIndex / maxFramesPerStack;
                frameIndexInStack = frameIndex % maxFramesPerStack;
            }
            else // doSaveLast
            {
                stackIndex =
                    (frameIndex - (frameCount - saveLast)) / maxFramesPerStack;
                frameIndexInStack =
                    (frameIndex - (frameCount - saveLast)) % maxFramesPerStack;
            }

            // First frame in new stack, close previous file and open new one
            if (frameIndexInStack == 0)
            {
                // Close previous file if some open
                if (file)
                {
                    file->Close();
                    delete file;
                    file = nullptr;
                }

                // Calculate number of frames in this file and set name
                fileName = ((saveDir.empty()) ? "." : saveDir) + "/";
                if (doSaveAll)
                {
                    if (stackIndex < (frameCount - 1) / maxFramesPerStack)
                        prdHeader.frameCount = maxFramesPerStack;
                    else
                        prdHeader.frameCount = ((frameCount - 1) % maxFramesPerStack) + 1;

                    fileName += "ss_stack_";
                }
                else if (doSaveFirst)
                {
                    if (stackIndex < (saveFirst - 1) / maxFramesPerStack)
                        prdHeader.frameCount = maxFramesPerStack;
                    else
                        prdHeader.frameCount = ((saveFirst - 1) % maxFramesPerStack) + 1;

                    fileName += "ss_stack_first_";
                }
                else // doSaveLast
                {
                    if (stackIndex < (saveLast - 1) / maxFramesPerStack)
                        prdHeader.frameCount = maxFramesPerStack;
                    else
                        prdHeader.frameCount = ((saveLast - 1) % maxFramesPerStack) + 1;

                    fileName += "ss_stack_last_";
                }
                fileName += std::to_string(stackIndex);

                // Add proper file extension and create its instance
                switch (storageType)
                {
                case StorageType::Prd:
                    fileName += ".prd";
                    file = new(std::nothrow) PrdFileSave(fileName, prdHeader);
                    break;
                case StorageType::Tiff:
                    fileName += ".tiff";
                    file = new(std::nothrow) TiffFileSave(fileName, prdHeader);
                    break;
                case StorageType::None:
                    break;
                // No default section, compiler will complain when new format added
                }

                // Open the file
                if (!file || !file->Open())
                {
                    Log::LogE("Error in opening file %s for frame with index %llu",
                            fileName.c_str(), (unsigned long long)frameIndex);
                    keepGoing = false;

                    delete file;
                    file = nullptr;
                }
            }

            // If some file is open store current frame in it
            if (file)
            {
                const Frame::Info& fi = frame->GetInfo();
                if (!file->WriteFrame(*frame, GetFrameExpTime(fi.GetFrameNr())))
                {
                    Log::LogE("Error in writing RAW data at %s for frame with index %llu",
                            fileName.c_str(), (unsigned long long)frameIndex);
                    keepGoing = false;
                }
                else
                {
                    m_toBeSavedFramesSaved++;
                }
            }
        }

        if (!keepGoing)
            RequestAbort();

        UnuseFrame(std::move(frame));

        frameIndex++;
    }

    // Just to be sure, close last file if remained open
    if (file)
    {
        file->Close();
        delete file;
    }
}

void pm::Acquisition::UpdateThreadLoop()
{
    const std::vector<std::string> progress{ "|", "/", "-", "\\" };
    size_t progressIndex = 0;
    size_t maxRefreshCounter = 0;

    while (!(m_acqThreadDoneFlag && m_diskThreadDoneFlag))
    {
        // Use wait_for instead of sleep to stop immediately on request
        {
            std::unique_lock<std::mutex> lock(m_updateThreadMutex);
            m_updateThreadCond.wait_for(lock, std::chrono::milliseconds(500), [this]() {
                return (m_acqThreadDoneFlag && m_diskThreadDoneFlag);
            });
        }
        if (m_acqThreadDoneFlag && m_diskThreadDoneFlag)
            break;

        // Don't update limits too often
        maxRefreshCounter++;
        if ((maxRefreshCounter % 8 == 0) && !m_acqThreadDoneFlag)
        {
            UpdateToBeSavedFramesMax();
        }

        // Print info about progress
        std::ostringstream ss;
        ss << progress[progressIndex] << " so far caught "
            << m_toBeProcessedFramesValid + m_toBeProcessedFramesLost
            << " frames";
        if (m_toBeProcessedFramesLost > 0)
            ss << " (" << m_toBeProcessedFramesLost << " lost)";
        ss << ", " << m_toBeProcessedFramesValid
            << " queued for processing";
        if (m_toBeSavedFramesLost > 0)
            ss << " (" << m_toBeSavedFramesLost << " dropped)";
        ss << ", " << m_toBeSavedFramesValid << " processed";
        ss << ", " << m_toBeSavedFramesSaved << " saved";

        if (m_diskThreadAbortFlag)
            ss << ", aborting...";
        else if (m_acqThreadAbortFlag)
            ss << ", finishing...";

        Log::LogP(ss.str());

        progressIndex = (progressIndex + 1) % progress.size();
    }
}

void pm::Acquisition::PrintAcqThreadStats() const
{
    const size_t frameCount = m_toBeProcessedFramesValid + m_toBeProcessedFramesLost;
    const double frameDropsPercent = (frameCount > 0)
        ? ((double)m_uncaughtFrames.GetCount() / (double)frameCount) * 100
        : 0.0;
    const double fps = (m_acqTime > 0.0)
        ? (double)m_toBeProcessedFramesValid / m_acqTime
        : 0.0;
    const double MiBps =
        round(fps * m_camera->GetFrameAcqCfg().GetFrameBytes() * 10 / 1024 / 1024) / 10.0;

    std::ostringstream ss;
    ss << "\nAcquisition thread queue stats:"
        << "\n    Frame count = " << frameCount
        << "\n  # Frame drops = " << m_uncaughtFrames.GetCount()
        << "\n  % Frame drops = " << frameDropsPercent
        << "\n  Average # frames between drops = " << m_uncaughtFrames.GetAvgSpacing()
        << "\n  Longest series of dropped frames = " << m_uncaughtFrames.GetLargestCluster()
        << "\n  Max. used frames = " << m_toBeProcessedFramesMaxPeak
        << " out of " << m_toBeProcessedFramesMax
        << "\n  Acquisition ran with " << fps << " fps (~" << MiBps << "MiB/s)";
    if (m_outOfOrderFrameCount > 0)
    {
        ss << "\n  " << m_outOfOrderFrameCount
            << " frames with frame number <= last stored frame number";
    }
    ss << "\n";

    Log::LogI(ss.str());
}

void pm::Acquisition::PrintDiskThreadStats() const
{
    const size_t frameCount = m_toBeSavedFramesValid + m_toBeSavedFramesLost;
    const double frameDropsPercent = (frameCount > 0)
        ? ((double)m_unsavedFrames.GetCount() / (double)frameCount) * 100
        : 0.0;
    const double fps = (m_diskTime > 0.0)
        ? (double)m_toBeSavedFramesValid / m_diskTime
        : 0.0;
    const double MiBps =
        round(fps * m_camera->GetFrameAcqCfg().GetFrameBytes() * 10 / 1024 / 1024) / 10.0;

    std::ostringstream ss;
    ss << "\nProcessing thread queue stats:"
        << "\n    Frame count = " << frameCount
        << "\n  # Frame drops = " << m_unsavedFrames.GetCount()
        << "\n  % Frame drops = " << frameDropsPercent
        << "\n  Average # frames between drops = " << m_unsavedFrames.GetAvgSpacing()
        << "\n  Longest series of dropped frames = " << m_unsavedFrames.GetLargestCluster()
        << "\n  Max. used frames = " << m_toBeSavedFramesMaxPeak
        // m_toBeSavedFramesMax could be less than a peak which would confuse users
        //<< " out of " << m_toBeSavedFramesMax
        << "\n  Processing ran with " << fps << " fps (~" << MiBps << "MiB/s)\n";

    Log::LogI(ss.str());
}

uint32_t pm::Acquisition::GetFrameExpTime(uint32_t frameNr)
{
    const int32_t trigMode = m_camera->GetSettings().GetTrigMode();

    const bool isVtm = trigMode == VARIABLE_TIMED_MODE;

    if (isVtm)
    {
        const std::vector<uint16_t>& vtmExposures =
            m_camera->GetSettings().GetVtmExposures();
        // frameNr is 1-based, not 0-based
        const uint32_t vtmExpIndex = (frameNr - 1) % vtmExposures.size();
        const uint16_t vtmExpTime = vtmExposures.at(vtmExpIndex);
        return static_cast<uint32_t>(vtmExpTime);
    }

    return m_camera->GetSettings().GetExposure();
}
