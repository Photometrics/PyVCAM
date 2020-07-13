#include "RealCamera.h"

/* System */
#include <chrono>
#include <limits>
#include <thread>

/* Local */
#include "Frame.h"
#include "Log.h"

bool pm::RealCamera::s_isInitialized = false;

void PV_DECL pm::RealCamera::TimeLapseCallbackHandler(FRAME_INFO* frameInfo,
        void* RealCamera_pointer)
{
    RealCamera* cam = static_cast<RealCamera*>(RealCamera_pointer);
    cam->HandleTimeLapseEofCallback(frameInfo);
}

pm::RealCamera::RealCamera()
    : m_timeLapseFrameCount(0),
    m_timeLapseFuture(),
    m_callbackHandler(nullptr),
    m_callbackContext(nullptr),
    m_latestFrameInfo(nullptr)
{
}

pm::RealCamera::~RealCamera()
{
    StopExp();
    Close();
}

bool pm::RealCamera::Initialize()
{
    if (s_isInitialized)
        return true;

    if (PV_OK != pl_pvcam_init())
    {
        Log::LogE("Failure initializing PVCAM (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    uns16 version;
    if (PV_OK != pl_pvcam_get_ver(&version))
    {
        Log::LogE("Failure getting PVCAM version (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    Log::LogI("Using PVCAM version %u.%u.%u\n",
            (version >> 8) & 0xFF,
            (version >> 4) & 0x0F,
            (version >> 0) & 0x0F);

    s_isInitialized = true;
    return true;
}

bool pm::RealCamera::Uninitialize()
{
    if (!s_isInitialized)
        return true;

    if (PV_OK != pl_pvcam_uninit())
    {
        Log::LogE("Failure uninitializing PVCAM (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    s_isInitialized = false;
    return true;
}

bool pm::RealCamera::GetCameraCount(int16& count) const
{
    if (PV_OK != pl_cam_get_total(&count))
    {
        Log::LogE("Failure getting camera count (%s)",
                GetErrorMessage().c_str());
        return false;
    }
    return true;
}

bool pm::RealCamera::GetName(int16 index, std::string& name) const
{
    name.clear();

    char camName[CAM_NAME_LEN];
    if (PV_OK != pl_cam_get_name(index, camName))
    {
        Log::LogE("Failed to get name for camera at index %d (%s)", index,
                GetErrorMessage().c_str());
        return false;
    }

    name = camName;
    return true;
}

std::string pm::RealCamera::GetErrorMessage() const
{
    std::string message;

    char errMsg[ERROR_MSG_LEN] = "\0";
    const int16 code = pl_error_code();
    if (PV_OK != pl_error_message(code, errMsg))
    {
        message = std::string("Unable to get error message for error code ")
            + std::to_string(code);
    }
    else
    {
        message = errMsg;
    }

    return message;
}

bool pm::RealCamera::Open(const std::string& name)
{
    if (m_isOpen)
        return true;

    if (PV_OK != pl_create_frame_info_struct(&m_latestFrameInfo))
    {
        Log::LogE("Failure creating frame info structure (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    if (PV_OK != pl_cam_open((char*)name.c_str(), &m_hCam, OPEN_EXCLUSIVE))
    {
        Log::LogE("Failure opening camera '%s' (%s)", name.c_str(),
                GetErrorMessage().c_str());
        m_hCam = -1;
        pl_release_frame_info_struct(m_latestFrameInfo); // Ignore errors
        m_latestFrameInfo = nullptr;
        return false;
    }

    return Camera::Open(name);
}

bool pm::RealCamera::Close()
{
    if (!m_isOpen)
        return true;

    if (PV_OK != pl_cam_close(m_hCam))
    {
        Log::LogE("Failed to close camera, error ignored (%s)",
                GetErrorMessage().c_str());
        // Error ignored, need to uninitialize PVCAM anyway
        //return false;
    }

    if (PV_OK != pl_release_frame_info_struct(m_latestFrameInfo))
    {
        Log::LogE("Failure releasing frame info structure, error ignored (%s)",
                GetErrorMessage().c_str());
        // Error ignored, need to uninitialize PVCAM anyway
        //return false;
    }

    DeleteBuffers();

    m_hCam = -1;
    m_latestFrameInfo = nullptr;

    return Camera::Close();
}

bool pm::RealCamera::SetupExp(const SettingsReader& settings)
{
    if (!Camera::SetupExp(settings))
        return false;

    const uns32 acqFrameCount = m_settings.GetAcqFrameCount();
    const uns32 bufferFrameCount = m_settings.GetBufferFrameCount();
    const AcqMode acqMode = m_settings.GetAcqMode();

    uns32 exposure = m_settings.GetExposure();
    const int32 trigMode = m_settings.GetTrigMode();
    const int32 expOutMode = m_settings.GetExpOutMode();
    const int16 expMode = (int16)trigMode | (int16)expOutMode;

    const uns16 rgn_total = (uns16)m_settings.GetRegions().size();
    const rgn_type* rgn_array = m_settings.GetRegions().data();

    uns32 frameBytes = 0;
    uns32 bufferBytes = 0;

    switch (acqMode)
    {
    case AcqMode::SnapSequence:
        if (acqFrameCount > std::numeric_limits<uns16>::max())
        {
            Log::LogE("Too many frames in sequence (%u does not fit in 16 bits)",
                    acqFrameCount);
            return false;
        }
        if (PV_OK != pl_exp_setup_seq(m_hCam, (uns16)acqFrameCount,
                    rgn_total, rgn_array, expMode, exposure, &bufferBytes))
        {
            Log::LogE("Failed to setup sequence acquisition (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        frameBytes = bufferBytes / acqFrameCount;
        break;

    case AcqMode::SnapCircBuffer:
    case AcqMode::LiveCircBuffer:
        if (PV_OK != pl_exp_setup_cont(m_hCam, rgn_total, rgn_array, expMode,
                    exposure, &frameBytes, CIRC_OVERWRITE))
        {
            Log::LogE("Failed to setup continuous acquisition (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        break;

    case AcqMode::SnapTimeLapse:
    case AcqMode::LiveTimeLapse:
        // Fix exposure time in VTM mode, it must not be zero
        // TODO: Do the same for SMART streaming
        if (trigMode == VARIABLE_TIMED_MODE)
        {
            // The value does not matter but it must not be zero
            exposure = 1;
        }
        if (PV_OK != pl_exp_setup_seq(m_hCam, 1, rgn_total, rgn_array, expMode,
                    exposure, &frameBytes))
        {
            Log::LogE("Failed to setup time-lapse acquisition (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
        // This mode uses single frame acq. but we re-use all frames in our buffer
        break;
    }

    if (!AllocateBuffers(bufferFrameCount, frameBytes))
        return false;

    m_timeLapseFrameCount = 0;

    return true;
}

bool pm::RealCamera::StartExp(CallbackEx3Fn callbackHandler, void* callbackContext)
{
    if (!callbackHandler || !callbackContext)
        return false;

    m_callbackHandler = callbackHandler;
    m_callbackContext = callbackContext;

    const AcqMode acqMode = m_settings.GetAcqMode();
    const int32 trigMode = m_settings.GetTrigMode();

    if (acqMode == AcqMode::SnapTimeLapse || acqMode == AcqMode::LiveTimeLapse)
    {
        /* Register time lapse callback only at the beginning, that might
           increase performance a bit. */
        if (m_timeLapseFrameCount == 0)
        {
            if (PV_OK != pl_cam_register_callback_ex3(m_hCam, PL_CALLBACK_EOF,
                    (void*)&RealCamera::TimeLapseCallbackHandler, (void*)this))
            {
                Log::LogE("Failed to register EOF callback for time-lapse mode (%s)",
                        GetErrorMessage().c_str());
                return false;
            }
        }
    }
    else
    {
        if (PV_OK != pl_cam_register_callback_ex3(m_hCam, PL_CALLBACK_EOF,
                (void*)m_callbackHandler, m_callbackContext))
        {
            Log::LogE("Failed to register EOF callback (%s)",
                    GetErrorMessage().c_str());
            return false;
        }
    }

    const uns32 frameBytes = (uns32)m_frameAcqCfg.GetFrameBytes();

    // Tell the camera to start
    bool keepGoing = false;
    switch (acqMode)
    {
    case AcqMode::SnapCircBuffer:
    case AcqMode::LiveCircBuffer:
        keepGoing = (PV_OK == pl_exp_start_cont(m_hCam, m_buffer,
                m_frameCount * frameBytes));
        break;
    case AcqMode::SnapSequence:
        keepGoing = (PV_OK == pl_exp_start_seq(m_hCam, m_buffer));
        break;
    case AcqMode::SnapTimeLapse:
    case AcqMode::LiveTimeLapse:
        // Update exposure time in VTM mode
        if (trigMode == VARIABLE_TIMED_MODE)
        {
            const uns32 vtmExpIndex =
                m_timeLapseFrameCount % m_settings.GetVtmExposures().size();
            const uns16 expTime = m_settings.GetVtmExposures().at(vtmExpIndex);
            if (PV_OK != pl_set_param(m_hCam, PARAM_EXP_TIME, (void*)&expTime))
            {
                Log::LogE("Failed to set new VTM exposure to %u (%s)", expTime,
                        GetErrorMessage().c_str());
                return false;
            }
        }
        // Re-use internal buffer for buffering when sequence has one frame only
        const uns32 frameIndex =
            m_timeLapseFrameCount % m_settings.GetBufferFrameCount();
        keepGoing = (PV_OK == pl_exp_start_seq(m_hCam,
                (void*)((uns8*)m_buffer + frameBytes * frameIndex)));
        break;
    }
    if (!keepGoing)
    {
        Log::LogE("Failed to start the acquisition (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    m_isImaging = true;

    return true;
}

bool pm::RealCamera::StopExp()
{
    bool ok = true;

    if (m_isImaging)
    {
        // Unconditionally stop the acquisition

        if (PV_OK != pl_exp_abort(m_hCam, CCS_HALT))
        {
            Log::LogE("Failed to abort acquisition, error ignored (%s)",
                    GetErrorMessage().c_str());
            // Error ignored, need to abort as much as possible
            //return false;
            ok = false;
        }
        if (PV_OK != pl_exp_finish_seq(m_hCam, m_buffer, 0))
        {
            Log::LogE("Failed to finish sequence, error ignored (%s)",
                    GetErrorMessage().c_str());
            // Error ignored, need to abort as much as possible
            //return false;
            ok = false;
        }

        m_isImaging = false;

        // Do not deregister callbacks before pl_exp_abort, abort could freeze then
        if (PV_OK != pl_cam_deregister_callback(m_hCam, PL_CALLBACK_EOF))
        {
            Log::LogE("Failed to deregister EOF callback, error ignored (%s)",
                    GetErrorMessage().c_str());
            // Error ignored, need to abort as much as possible
            //return false;
            ok = false;
        }

        m_callbackHandler = nullptr;
        m_callbackContext = nullptr;
    }

    return ok;
}

pm::Camera::AcqStatus pm::RealCamera::GetAcqStatus() const
{
    if (!m_isImaging)
        return Camera::AcqStatus::Inactive;

    const AcqMode acqMode = m_settings.GetAcqMode();
    const bool isLive =
        acqMode == AcqMode::SnapCircBuffer || acqMode == AcqMode::LiveCircBuffer;

    int16 status;
    uns32 bytes_arrived;
    uns32 buffer_cnt;

    const rs_bool res = (isLive)
            ? pl_exp_check_cont_status(m_hCam, &status, &bytes_arrived, &buffer_cnt)
            : pl_exp_check_status(m_hCam, &status, &bytes_arrived);

    if (res == PV_FAIL)
        return Camera::AcqStatus::Failure;

    switch (status)
    {
    case READOUT_NOT_ACTIVE:
        return Camera::AcqStatus::Inactive;
    case EXPOSURE_IN_PROGRESS:
    case READOUT_IN_PROGRESS:
    case FRAME_AVAILABLE: // READOUT_COMPLETE
        return (isLive)
            ? Camera::AcqStatus::Active // FRAME_AVAILABLE
            : Camera::AcqStatus::Inactive; // READOUT_COMPLETE
    case READOUT_FAILED:
    default:
        return Camera::AcqStatus::Failure;
    }
}

bool pm::RealCamera::SetParam(uns32 id, void* param)
{
    return (PV_OK == pl_set_param(m_hCam, id, param));
}

bool pm::RealCamera::GetParam(uns32 id, int16 attr, void* param) const
{
    return (PV_OK == pl_get_param(m_hCam, id, attr, param));
}

bool pm::RealCamera::GetEnumParam(uns32 id, std::vector<EnumItem>& items) const
{
    items.clear();

    uns32 count;
    if (PV_OK != pl_get_param(m_hCam, id, ATTR_COUNT, &count))
        return false;
    for (uns32 n = 0; n < count; n++) {
        uns32 enumStrLen;
        if (PV_OK != pl_enum_str_length(m_hCam, id, n, &enumStrLen))
            return false;

        int32 enumValue;
        char* enumString = new(std::nothrow) char[enumStrLen];
        if (PV_OK != pl_get_enum_param(m_hCam, id, n, &enumValue, enumString,
                    enumStrLen)) {
            return false;
        }
        EnumItem item;
        item.value = enumValue;
        item.desc = enumString;
        items.push_back(item);
        delete [] enumString;
    }

    return true;
}

bool pm::RealCamera::GetLatestFrame(Frame& frame) const
{
    // Set to an error state before PVCAM tries to reset pointer to valid frame location
    void* data = nullptr;

    // Get the latest frame
    if (PV_OK != pl_exp_get_latest_frame_ex(m_hCam, &data, m_latestFrameInfo))
    {
        Log::LogE("Failed to get latest frame from PVCAM (%s)",
                GetErrorMessage().c_str());
        return false;
    }

    if (!data)
    {
        Log::LogE("Invalid latest frame pointer");
        return false;
    }

    // Fix the frame number which is always 1 in time lapse mode
    const AcqMode acqMode = m_settings.GetAcqMode();
    if (acqMode == AcqMode::SnapTimeLapse || acqMode == AcqMode::LiveTimeLapse)
    {
        m_latestFrameInfo->FrameNr = (int32)m_timeLapseFrameCount;
    }

    const size_t frameBytes = m_frameAcqCfg.GetFrameBytes();
    const size_t offset = ((uns8*)data - (uns8*)m_buffer);
    const size_t index = offset / frameBytes;
    if (index * frameBytes != offset)
    {
        Log::LogE("Invalid frame data offset");
        return false;
    }

    if (m_frames[index]->GetData() != data)
    {
        Log::LogE("Frame data address does not match");
        return false;
    }

    m_frames[index]->Invalidate();
    frame.Invalidate();

    const uint32_t oldFrameNr = m_frames[index]->GetInfo().GetFrameNr();
    const Frame::Info fi(
            (uint32_t)m_latestFrameInfo->FrameNr,
            (uint64_t)m_latestFrameInfo->TimeStampBOF,
            (uint64_t)m_latestFrameInfo->TimeStamp);
    m_frames[index]->SetInfo(fi);
    UpdateFrameIndexMap(oldFrameNr, index);

    return frame.Copy(*m_frames[index], false);
}

void pm::RealCamera::HandleTimeLapseEofCallback(FRAME_INFO* frameInfo)
{
    m_timeLapseFrameCount++;

    // Fix the frame number which is always 1 in time lapse mode
    frameInfo->FrameNr = (int32)m_timeLapseFrameCount;

    // Call registered callback
    m_callbackHandler(frameInfo, m_callbackContext);

    // Do not start acquisition for next frame if done
    if (m_timeLapseFrameCount >= m_settings.GetAcqFrameCount()
            && m_settings.GetAcqMode() != AcqMode::LiveTimeLapse)
        return;

    /* m_timeLapseFuture member exists only because of the following fact
       stated in specification:

           "If the std::future obtained from std::async has temporary object
           lifetime (not moved or bound to a variable), the destructor of the
           std::future will block at the end of the full expression until the
           asynchronous operation completes."

       We don't need to wait for any result and we are sure that the future
       either didn't start yet (in case of first frame) or has successfully
       completed (the single frame acq. started again 'cos we are here). */

    m_timeLapseFuture = std::async(std::launch::async, [this]() -> void {
        // Backup callback data, StopExp will clear these members
        CallbackEx3Fn callbackHandler = m_callbackHandler;
        void* callbackContext = m_callbackContext;

        /* No need to stop explicitely, acquisition has already finished and
           we don't want to release allocated buffer. */
        //StopExp();

        if (m_settings.GetTimeLapseDelay() > 0)
            std::this_thread::sleep_for(
                    std::chrono::milliseconds(m_settings.GetTimeLapseDelay()));

        if (!StartExp(callbackHandler, callbackContext))
        {
            /* There is no direct way how to let Acquisition know about error.
               Call the callback handler with null pointer again. */
            m_callbackHandler(nullptr, m_callbackContext);
        }
    });
}
