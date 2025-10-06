// Python
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

// PVCAM
#include <master.h>
#include <pvcam.h>

// System
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <Windows.h>
    #include <malloc.h> // _aligned_malloc
    using FileHandle = HANDLE;
    const/*expr*/ auto cInvalidFileHandle = (FileHandle)INVALID_HANDLE_VALUE;
#else
    #include <stdlib.h> // aligned_alloc
    #include <sys/types.h> // open
    #include <sys/stat.h> // open
    #include <fcntl.h> // open
    #include <unistd.h> // close, write, sysconf
    using FileHandle = int;
    constexpr auto cInvalidFileHandle = (FileHandle)-1;
#endif

// Local constants

static constexpr uns16 MAX_ROIS = 512; // Max 15 ROIs, but up to 512 centroids
static constexpr uns32 ALIGNMENT_BOUNDARY = 4096;

// Local types

struct AcqBuffer
{
    AcqBuffer(size_t size)
        : size(size)
    {
        // Always align frameBuffer on a page boundary.
        // This is required for non-buffered streaming to disk.
#ifdef _WIN32
        data = _aligned_malloc(size, ALIGNMENT_BOUNDARY);
#else
        data = aligned_alloc(ALIGNMENT_BOUNDARY, size);
#endif
        if (!data)
            throw std::bad_alloc();
    }

    ~AcqBuffer()
    {
#ifdef _WIN32
        _aligned_free(data);
#else
        free(data);
#endif
    }

    void* data{ NULL };
    size_t size;
};

struct Frame
{
    void* address{ NULL }; // Address within AcqBuffer received from PVCAM
    uns32 count{ 0 }; // Frame number that resets after every setup
    uns32 nr{ 0 }; // FrameNr from PVCAM's FRAME_INFO structure
};

union ParamValue
{
    char val_str[MAX_PP_NAME_LEN];
    int32 val_enum;
    int8 val_int8;
    uns8 val_uns8;
    int16 val_int16;
    uns16 val_uns16;
    int32 val_int32;
    uns32 val_uns32;
    long64 val_long64;
    ulong64 val_ulong64;
    flt32 val_flt32;
    flt64 val_flt64;
    rs_bool val_bool;
    rgn_type val_roi;
    smart_stream_type val_ss;
};

class Camera
{
public:
    Camera()
    {
        if (!pl_md_create_frame_struct_cont(&m_mdFrame, MAX_ROIS))
            throw std::bad_alloc();
    }

    ~Camera()
    {
        UnsetStreamToDisk();
        ReleaseAcqBuffer();
        pl_md_release_frame_struct(m_mdFrame); // Ignore PVCAM errors
    }

    bool AllocateAcqBuffer(uns32 frameCount, uns32 frameBytes)
    {
        // PVCAM supports buffer up to 4GB only
        const uint64_t bufferBytes64 = (uint64_t)frameBytes * frameCount;
        if (bufferBytes64 > (std::numeric_limits<uns32>::max)())
            return false;

        if (m_acqBuffer && m_acqBuffer->size == bufferBytes64)
            return true; // Already allocated

        ReleaseAcqBuffer();

        try
        {
            auto acqBuffer = std::make_shared<AcqBuffer>(bufferBytes64);

            m_acqBuffer = acqBuffer;
            m_frameCount = frameCount;
            m_frameBytes = frameBytes;
        }
        catch (const std::bad_alloc& /*ex*/)
        {
            return false;
        }

        return true;
    }

    void ReleaseAcqBuffer()
    {
        m_acqBuffer.reset(); // Drop buffer ownership
        m_frameCount = 0;
        m_frameBytes = 0;
    }

    bool SetStreamToDisk(const char* streamToDiskPath)
    {
        if (!streamToDiskPath)
            return true;

        if (m_streamFileHandle != cInvalidFileHandle)
            return false; // Already streaming

        //printf("Stream to disk path set: '%s'\n", streamToDiskPath);

#ifdef _WIN32
        const int flags = FILE_FLAG_NO_BUFFERING;
        m_streamFileHandle = ::CreateFileA(
                streamToDiskPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, flags, NULL);
#else
        // The O_DIRECT flag is the key on Linux
        const int flags = O_DIRECT | (O_WRONLY | O_CREAT | O_TRUNC);
        const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        m_streamFileHandle = ::open(streamToDiskPath, flags, mode);
#endif
        if (m_streamFileHandle == cInvalidFileHandle)
            return false;

        m_readIndex = 0;
        m_frameResidual = 0;

        return true;
    }

    bool StreamFrameToDisk(void* frameAddress)
    {
        if (m_streamFileHandle == cInvalidFileHandle)
            return true;

        // When streaming to a file, we must always write to an alignment boundary.
        // Since the frame may not be a multiple of the alignment boundary,
        // we might not write the entire frame. The remainder of the frame will
        // be written on the next pass through this function. When we reach the end
        // of the circular buffer we may need to pad additional bytes onto the end
        // of the frame to meet the alignment requirement. These bytes are to be
        // discarded later when using the file.

        // TODO: What? EOF callback is invoked for every frame, one by one
        // Frames may arrive out of order.
        // To handle this situation, m_readIndex is used to track which bytes
        // stream to file next rather than the pointer returned by PVCAM

        uns32 availableBytes = m_frameResidual + m_frameBytes;

        // This routine may fall behind writing the latest data.
        // Attempt to catch up when the frameAddress is ahead of the
        // read index by more than a frame.
        const uintptr_t availableIndex =
            (uintptr_t)frameAddress - (uintptr_t)m_acqBuffer->data;
        if (availableIndex > m_readIndex)
        {
            if (availableBytes < availableIndex - m_readIndex)
            {
                availableBytes = (uns32)(availableIndex - m_readIndex);
            }
        }

        uns32 bytesToWrite = (availableBytes / ALIGNMENT_BOUNDARY) * ALIGNMENT_BOUNDARY;
        const bool lastFrameInBuffer =
            (m_acqBuffer->size - m_readIndex - bytesToWrite) < ALIGNMENT_BOUNDARY;
        if (lastFrameInBuffer)
        {
            bytesToWrite += ALIGNMENT_BOUNDARY;
        }

        void* alignedFrameData = reinterpret_cast<uns8*>(m_acqBuffer->data) + m_readIndex;
        uns32 bytesWritten = 0;
#ifdef _WIN32
        ::WriteFile(m_streamFileHandle, alignedFrameData, (DWORD)bytesToWrite,
                (LPDWORD)&bytesWritten, NULL);
#else
        bytesWritten += ::write(m_streamFileHandle, alignedFrameData, bytesToWrite);
#endif
        if (bytesWritten != bytesToWrite)
        {
            m_acqCbError =
                std::string("Streaming to disk failed, not all bytes written")
                + " - expected " + std::to_string(bytesToWrite)
                + " but written " + std::to_string(bytesWritten) + ".";
            return false;
        }

        // Store the count of frame bytes not written.
        // Increment read index or reset to start of frame buffer if needed
        m_frameResidual = (lastFrameInBuffer) ? 0 : availableBytes - bytesWritten;
        m_readIndex = (lastFrameInBuffer) ? 0 : m_readIndex + bytesWritten;

        return true;
    }

    bool UnsetStreamToDisk()
    {
        if (m_streamFileHandle == cInvalidFileHandle)
            return true;

        bool writeOk = true;

        if (m_frameResidual != 0)
        {
            void* alignedFrameData =
                reinterpret_cast<uns8*>(m_acqBuffer->data) + m_readIndex;
            uns32 bytesWritten = 0;
#ifdef _WIN32
            ::WriteFile(m_streamFileHandle, alignedFrameData,
                    (DWORD)ALIGNMENT_BOUNDARY, (LPDWORD)&bytesWritten, NULL);
#else
            bytesWritten +=
                ::write(m_streamFileHandle, alignedFrameData, ALIGNMENT_BOUNDARY);
#endif
            if (bytesWritten != ALIGNMENT_BOUNDARY)
            {
                // Set error but complete the cleanup first
                m_acqCbError =
                    std::string("Streaming to disk failed, not all bytes written")
                    + " - expected " + std::to_string(ALIGNMENT_BOUNDARY)
                    + " but written " + std::to_string(bytesWritten) + ".";
                writeOk = false;
            }
        }

#ifdef _WIN32
        ::CloseHandle(m_streamFileHandle);
#else
        ::close(m_streamFileHandle);
#endif
        m_streamFileHandle = cInvalidFileHandle;

        return writeOk;
    }

public:
    std::mutex m_mutex{};

    // Reference-counted ownership of the acq. buffer shared with NumPy objects
    // to guarantee the buffer is valid until last capsule releases it.
    std::shared_ptr<AcqBuffer> m_acqBuffer{ NULL };
    uns32 m_frameCount{ 0 };
    uns32 m_frameBytes{ 0 };

    std::chrono::time_point<std::chrono::high_resolution_clock> m_fpsLastTime{
        std::chrono::high_resolution_clock::now() };
    double m_fps{ 0.0 };
    uns32 m_fpsFrameCnt{ 0 };

    bool m_isSequence{ false };

    std::condition_variable m_acqCond{};
    std::queue<Frame> m_acqQueue{};
    size_t m_acqQueueCapacity{ 0 };
    bool m_acqAbort{ false };
    bool m_acqNewFrame{ false };
    uns32 m_acqFrameCnt{ 0 };
    std::string m_acqCbError{};

    // Metadata objects
    bool m_metadataEnabled{ false };
    md_frame* m_mdFrame{ NULL };

    // Stream to disk
    FileHandle m_streamFileHandle{ cInvalidFileHandle };
    uns32 m_readIndex{ 0 }; // Position in m_acqBuffer to save data from
    uns32 m_frameResidual{ 0 };
};

// Global variables

std::map<int16, std::shared_ptr<Camera>> g_cameraMap{}; // The key is hcam
std::mutex                               g_cameraMapMutex{};

// Local functions

/** Helper that always returns NULL and raises ValueError "Invalid parameters." message. */
static PyObject* ParamParseError()
{
    return PyErr_Format(PyExc_ValueError, "Invalid parameters.");
}

/** Helper that always returns NULL and raises RuntimeError with PVCAM error message. */
static PyObject* PvcamError()
{
    char errMsg[ERROR_MSG_LEN] = "<UNKNOWN ERROR>";
    pl_error_message(pl_error_code(), errMsg); // Ignore PVCAM error
    return PyErr_Format(PyExc_RuntimeError, errMsg);
}

/** Helper that returns Camera instance from global map, or NULL if doesn't exist. */
static std::shared_ptr<Camera> GetCamera(int16 hcam, bool setPyErr = true)
{
    std::shared_ptr<Camera> cam;
    try
    {
        std::lock_guard<std::mutex> lock(g_cameraMapMutex);
        cam = g_cameraMap.at(hcam);
    }
    catch (const std::out_of_range& ex)
    {
        if (setPyErr)
            PyErr_Format(PyExc_KeyError, "Invalid camera handle (%s).", ex.what());
        return NULL;
    }
    return cam;
}

/** Sets ValueError on error and returns rgn_type with all zeroes */
static rgn_type PopulateRegion(PyObject* roiObj)
{
    rgn_type roi{ 0, 0, 0, 0, 0, 0 };

    PyObject* s1Obj   = PyObject_GetAttrString(roiObj, "s1");
    PyObject* s2Obj   = PyObject_GetAttrString(roiObj, "s2");
    PyObject* sbinObj = PyObject_GetAttrString(roiObj, "sbin");
    PyObject* p1Obj   = PyObject_GetAttrString(roiObj, "p1");
    PyObject* p2Obj   = PyObject_GetAttrString(roiObj, "p2");
    PyObject* pbinObj = PyObject_GetAttrString(roiObj, "pbin");

    if (s1Obj && s2Obj && sbinObj && p1Obj && p2Obj && pbinObj)
    {
        const long s1   = PyLong_AsLong(s1Obj);
        const long s2   = PyLong_AsLong(s2Obj);
        const long sbin = PyLong_AsLong(sbinObj);
        const long p1   = PyLong_AsLong(p1Obj);
        const long p2   = PyLong_AsLong(p2Obj);
        const long pbin = PyLong_AsLong(pbinObj);
        if (       s1   >= 0 && s1   <= (long)(std::numeric_limits<uns16>::max)()
                && s2   >= 0 && s2   <= (long)(std::numeric_limits<uns16>::max)()
                && sbin >= 1 && sbin <= (long)(std::numeric_limits<uns16>::max)()
                && p1   >= 0 && p1   <= (long)(std::numeric_limits<uns16>::max)()
                && p2   >= 0 && p2   <= (long)(std::numeric_limits<uns16>::max)()
                && pbin >= 1 && pbin <= (long)(std::numeric_limits<uns16>::max)())
        {
            roi.s1   = (uns16)s1;
            roi.s2   = (uns16)s2;
            roi.sbin = (uns16)sbin;
            roi.p1   = (uns16)p1;
            roi.p2   = (uns16)p2;
            roi.pbin = (uns16)pbin;
        }
    }

    Py_XDECREF(s1Obj);
    Py_XDECREF(s2Obj);
    Py_XDECREF(sbinObj);
    Py_XDECREF(p1Obj);
    Py_XDECREF(p2Obj);
    Py_XDECREF(pbinObj);

    return roi;
}

/** Sets ValueError on error and returns empty list */
static std::vector<rgn_type> PopulateRegions(PyObject* roiListObj)
{
    const Py_ssize_t count = PyList_Size(roiListObj);
    if (count <= 0 || count > (Py_ssize_t)(std::numeric_limits<uns16>::max)())
    {
        PyErr_Format(PyExc_ValueError, "Invalid ROI count (%zd).", count);
        return std::vector<rgn_type>();
    }

    auto ParseError = []() {
        // Override the error
        PyErr_Format(PyExc_ValueError, "Failed to parse ROI members.");
        return std::vector<rgn_type>();
    };

    std::vector<rgn_type> roiArray((size_t)count);
    for (Py_ssize_t i = 0; i < count; i++)
    {
        PyObject* roiObj  = PyList_GetItem(roiListObj, i);
        if (!roiObj)
            return ParseError();

        rgn_type roi = PopulateRegion(roiObj);
        if (roi.sbin == 0) // Invalid roi has all zeroes, let's check sbin only
            return ParseError();

        roiArray[i] = roi;
    }
    return roiArray;
}

/** Sets ValueError on error */
static std::vector<uns32> PopulateSsParams(PyObject* ssListObj)
{
    const Py_ssize_t count = PyList_Size(ssListObj);
    if (count < 0 || count > (Py_ssize_t)(std::numeric_limits<uns16>::max)())
    {
        PyErr_Format(PyExc_ValueError, "Invalid SMART item count (%zd).", count);
        return std::vector<uns32>();
    }

    auto ParseError = []() {
        // Override the error
        PyErr_Format(PyExc_ValueError, "Failed to parse SMART items.");
        return std::vector<uns32>();
    };

    std::vector<uns32> ssArray((size_t)count);
    for (Py_ssize_t i = 0; i < count; i++)
    {
        PyObject* ssObj  = PyList_GetItem(ssListObj, i);
        if (!ssObj)
            return ParseError();

        uns32 ssItem = (uns32)PyLong_AsUnsignedLong(ssObj);
        if (PyErr_Occurred())
            return ParseError();

        ssArray[i] = ssItem;
    }
    return ssArray;
}

static void NewFrameHandler(FRAME_INFO* pFrameInfo, void* context)
{
    std::shared_ptr<Camera> cam = GetCamera(pFrameInfo->hCam, false);
    if (!cam)
    {
        fprintf(stderr, "pvc.NewFrameHandler: Invalid camera instance key.\n");
        return; // No 'cam' means no mutex lock and no notify_all
    }

    void* address;
    FRAME_INFO fi;
    const rs_bool getLatestFrameResult =
        pl_exp_get_latest_frame_ex(pFrameInfo->hCam, &address, &fi);

    std::lock_guard<std::mutex> lock(cam->m_mutex);

    cam->m_acqFrameCnt++;
    //printf("New frame callback. Frame count %u\n", cam->m_acqFrameCnt);

    // Re-compute FPS every 5 frames
    constexpr uns32 FPS_FRAME_COUNT = 5;
    if (++cam->m_fpsFrameCnt >= FPS_FRAME_COUNT)
    {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto timeDeltaUs =
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - cam->m_fpsLastTime).count();
        if (timeDeltaUs != 0)
        {
            cam->m_fps = (double)cam->m_fpsFrameCnt / timeDeltaUs * 1e6;
            cam->m_fpsLastTime = now;
            cam->m_fpsFrameCnt = 0;
            //printf("FPS: %.1f timeDeltaUs: %lld\n", cam->m_fps, timeDeltaUs);
        }
    }

    if (!getLatestFrameResult)
    {
        cam->m_acqCbError = "Failed to get latest frame from PVCAM.";
        cam->m_acqCond.notify_all(); // Wakeup get_frame if anybody waits
        return;
    }

    Frame frame;
    frame.address = address;
    frame.count = cam->m_acqFrameCnt;
    frame.nr = (uns32)fi.FrameNr;

    // Add frame to the queue, pop the oldest once the capacity is reached
    while (cam->m_acqQueue.size() >= cam->m_acqQueueCapacity)
    {
        //printf("Dropped frame nr %u in favor of nr %u\n",
        //        cam->m_acqQueue.front().nr, frame.nr);
        cam->m_acqQueue.pop();
    }
    cam->m_acqQueue.push(frame);
    cam->m_acqNewFrame = true;

    if (cam->m_streamFileHandle != cInvalidFileHandle)
    {
        if (!cam->StreamFrameToDisk(frame.address))
        {
            // cam->m_acqCbError already set in StreamFrameToDisk()
            cam->m_acqCond.notify_all(); // Wakeup get_frame if anybody waits
            return;
        }
    }

    cam->m_acqCond.notify_all(); // Wakeup get_frame if anybody waits
}

// Module functions

/** Initializes PVCAM. */
static PyObject* pvc_init_pvcam(PyObject* self, PyObject* args)
{
    if(!pl_pvcam_init())
        return PvcamError();

    Py_RETURN_NONE;
}

/** Uninitializes PVCAM. */
static PyObject* pvc_uninit_pvcam(PyObject* self, PyObject* args)
{
    if (!pl_pvcam_uninit())
        return PvcamError();

    Py_RETURN_NONE;
}

/** Returns a string with first three version numbers of the currently installed PVCAM. */
static PyObject* pvc_get_pvcam_version(PyObject* self, PyObject* args)
{
    uns16 verNum;
    if (!pl_pvcam_get_ver(&verNum))
        return PvcamError();

    char verStr[10]; // 3 + 1 + 2 + 1 + 2 + 1 characters
    sprintf(verStr, "%u.%u.%u", (verNum >> 8) & 0xFF,
                                (verNum >> 4) & 0x0F,
                                (verNum >> 0) & 0x0F);
    return PyUnicode_FromString(verStr);
}

/** Returns a string with major and minor version numbers of the camera firmware. */
static PyObject* pvc_get_cam_fw_version(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    uns16 verNum;
    if (!pl_get_param(hcam, PARAM_CAM_FW_VERSION, ATTR_CURRENT, &verNum))
        return PvcamError();

    char verStr[8]; // 3 + 1 + 3 + 1 characters
    sprintf(verStr, "%u.%u", (verNum >> 8) & 0xFF,
                             (verNum >> 0) & 0xFF);
    return PyUnicode_FromString(verStr);
}

/** Returns the number of available cameras currently connected to the system. */
static PyObject* pvc_get_cam_total(PyObject* self, PyObject* args)
{
    int16 count;
    if (!pl_cam_get_total(&count))
        return PvcamError();

    return PyLong_FromLong(count);
}

/** Returns a Python String containing the name of the camera  given its index. */
static PyObject* pvc_get_cam_name(PyObject* self, PyObject* args)
{
    int16 camIndex;
    if (!PyArg_ParseTuple(args, "h", &camIndex))
        return ParamParseError();

    char camName[CAM_NAME_LEN];
    if (!pl_cam_get_name(camIndex, camName))
        return PvcamError();

    return PyUnicode_FromString(camName);
}

/** Opens a camera given its name and return camera handle. */
static PyObject* pvc_open_camera(PyObject* self, PyObject* args)
{
    char* camName;
    if (!PyArg_ParseTuple(args, "s", &camName))
        return ParamParseError();

    int16 hcam;
    if (!pl_cam_open(camName, &hcam, OPEN_EXCLUSIVE))
        return PvcamError();

    std::shared_ptr<Camera> cam;
    try
    {
        cam = std::make_shared<Camera>();
    }
    catch (const std::bad_alloc& ex)
    {
        pl_cam_close(hcam); // Ignore PVCAM errors
        return PyErr_Format(PyExc_MemoryError,
                "Unable to allocate new Camera instance (%s).", ex.what());
    }

    // Successfully opened, save new Camera instance to global map
    {
        std::lock_guard<std::mutex> lock(g_cameraMapMutex);
        g_cameraMap[hcam] = cam;
    }
    return PyLong_FromLong(hcam);
}

/** Closes a camera given its handle. */
static PyObject* pvc_close_camera(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    if (!pl_cam_close(hcam))
        return PvcamError();

    // Successfully closed, remove Camera instance from global map
    {
        std::lock_guard<std::mutex> lock(g_cameraMapMutex);
        g_cameraMap.erase(hcam);
    }
    Py_RETURN_NONE;
}

/** Returns the value of the specified parameter. */
static PyObject* pvc_get_param(PyObject* self, PyObject* args)
{
    int16 hcam;
    uns32 paramId;
    int16 paramAttr;
    if (!PyArg_ParseTuple(args, "hIh", &hcam, &paramId, &paramAttr))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();
    if (paramAttr == ATTR_AVAIL)
        return PyBool_FromLong(avail);
    if (!avail)
        return PyErr_Format(PyExc_AttributeError,
                "Invalid setting for this camera. Parameter ID 0x%08X is not available.",
                paramId);

    uns16 paramType;
    if (!pl_get_param(hcam, paramId, ATTR_TYPE, &paramType))
        return PvcamError();

    ParamValue paramValue;
    std::vector<uns32> ssItems; // Helper container to hold data for SMART streaming
    if (paramType == TYPE_SMART_STREAM_TYPE_PTR)
    {
        switch (paramAttr)
        {
        case ATTR_CURRENT:
        case ATTR_DEFAULT:
        case ATTR_MIN:
        case ATTR_MAX:
        case ATTR_INCREMENT:
            if (!pl_get_param(hcam, paramId, ATTR_MAX, &paramValue.val_ss.entries))
                return PvcamError();
            ssItems.resize(paramValue.val_ss.entries);
            paramValue.val_ss.params = ssItems.data();
            break;
        default:
            break;
        }
    }

    if (!pl_get_param(hcam, paramId, paramAttr, &paramValue))
        return PvcamError();

    switch (paramAttr)
    {
    //case ATTR_AVAIL: // Already handled above
    case ATTR_LIVE:
        return PyBool_FromLong(paramValue.val_bool);
    case ATTR_TYPE:
    case ATTR_ACCESS:
        return PyLong_FromUnsignedLong(paramValue.val_uns16);
    case ATTR_COUNT:
        return PyLong_FromUnsignedLong(paramValue.val_uns32);
    case ATTR_CURRENT:
        break; // Handle all param types below
    case ATTR_DEFAULT:
    case ATTR_MIN:
    case ATTR_MAX:
    case ATTR_INCREMENT:
        if (paramType == TYPE_SMART_STREAM_TYPE_PTR)
            for (uns32 i = 0; i < paramValue.val_ss.entries; i++)
                paramValue.val_ss.params[i] = 0;
        break; // Continue handling as other param types below
    default:
        return PyErr_Format(PyExc_RuntimeError,
                "Failed to match parameter attribute (%u).", (uns32)paramAttr);
    }

    switch (paramType)
    {
    case TYPE_CHAR_PTR:
        return PyUnicode_FromString(paramValue.val_str);
    case TYPE_ENUM:
        return PyLong_FromLong(paramValue.val_enum);
    case TYPE_INT8:
        return PyLong_FromLong(paramValue.val_int8);
    case TYPE_UNS8:
        return PyLong_FromUnsignedLong(paramValue.val_uns8);
    case TYPE_INT16:
        return PyLong_FromLong(paramValue.val_int16);
    case TYPE_UNS16:
        return PyLong_FromUnsignedLong(paramValue.val_uns16);
    case TYPE_INT32:
        return PyLong_FromLong(paramValue.val_int32);
    case TYPE_UNS32:
        return PyLong_FromUnsignedLong(paramValue.val_uns32);
    case TYPE_INT64:
        return PyLong_FromLongLong(paramValue.val_long64);
    case TYPE_UNS64:
        return PyLong_FromUnsignedLongLong(paramValue.val_ulong64);
    case TYPE_FLT32:
        return PyFloat_FromDouble(paramValue.val_flt32);
    case TYPE_FLT64:
        return PyFloat_FromDouble(paramValue.val_flt64);
    case TYPE_BOOLEAN:
        return PyBool_FromLong(paramValue.val_bool);
    case TYPE_SMART_STREAM_TYPE_PTR: {
        PyObject* pySsList = PyList_New(paramValue.val_ss.entries);
        if (!pySsList)
            return NULL;
        for (uns32 i = 0; i < paramValue.val_ss.entries; i++)
        {
            PyObject* pySsItem = PyLong_FromUnsignedLong(paramValue.val_ss.params[i]);
            if (!pySsItem)
            {
                Py_DECREF(pySsList);
                return NULL;
            }
            PyList_SET_ITEM(pySsList, (Py_ssize_t)i, pySsItem);
        }
        return pySsList;
    }
    case TYPE_RGN_TYPE:
        // Matches format in metadata and is compatible with Camera.RegionOfInterest
        return Py_BuildValue("{s:H,s:H,s:H,s:H,s:H,s:H}", // dict
                "s1",   paramValue.val_roi.s1,
                "s2",   paramValue.val_roi.s2,
                "sbin", paramValue.val_roi.sbin,
                "p1",   paramValue.val_roi.p1,
                "p2",   paramValue.val_roi.p2,
                "pbin", paramValue.val_roi.pbin);
    case TYPE_SMART_STREAM_TYPE: // Not used in PVCAM
    case TYPE_VOID_PTR: // Not used in PVCAM
    case TYPE_VOID_PTR_PTR: // Not used in PVCAM
    case TYPE_RGN_TYPE_PTR: // Not used in PVCAM
    case TYPE_RGN_LIST_TYPE: // Not used in PVCAM
    case TYPE_RGN_LIST_TYPE_PTR: // Not used in PVCAM
    default:
        return PyErr_Format(PyExc_RuntimeError,
                "Failed to match parameter type (%u).", (uns32)paramType);
    }
}

/** Sets a specified parameter to a given value. */
static PyObject* pvc_set_param(PyObject* self, PyObject* args)
{
    int16 hcam;
    uns32 paramId;
    PyObject* paramValueObj;
    if (!PyArg_ParseTuple(args, "hIO", &hcam, &paramId, &paramValueObj))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();
    if (!avail)
        return PyErr_Format(PyExc_AttributeError,
                "Invalid setting for this camera. Parameter ID 0x%08X is not available.",
                paramId);

    uns16 paramType;
    if (!pl_get_param(hcam, paramId, ATTR_TYPE, &paramType))
        return PvcamError();

    ParamValue paramValue;
    std::vector<uns32> ssItems; // Helper container to hold data for SMART streaming
    switch (paramType)
    {
    case TYPE_CHAR_PTR: {
        Py_ssize_t size;
        const char* str = PyUnicode_AsUTF8AndSize(paramValueObj, &size);
        if (!str)
            return NULL;
        const size_t strLen = (std::min)((size_t)size, sizeof(paramValue.val_str));
        memcpy(paramValue.val_str, str, strLen);
        break;
    }
    case TYPE_ENUM:
        paramValue.val_enum = (int32)PyLong_AsLong(paramValueObj);
        break;
    case TYPE_INT8:
        paramValue.val_int8 = (int8)PyLong_AsLong(paramValueObj);
        break;
    case TYPE_UNS8:
        paramValue.val_uns8 = (uns8)PyLong_AsUnsignedLong(paramValueObj);
        break;
    case TYPE_INT16:
        paramValue.val_int16 = (int16)PyLong_AsLong(paramValueObj);
        break;
    case TYPE_UNS16:
        paramValue.val_uns16 = (uns16)PyLong_AsUnsignedLong(paramValueObj);
        break;
    case TYPE_INT32:
        paramValue.val_int32 = (int32)PyLong_AsLong(paramValueObj);
        break;
    case TYPE_UNS32:
        paramValue.val_uns32 = (uns32)PyLong_AsUnsignedLong(paramValueObj);
        break;
    case TYPE_INT64:
        paramValue.val_long64 = (long64)PyLong_AsLongLong(paramValueObj);
        break;
    case TYPE_UNS64:
        paramValue.val_ulong64 = (ulong64)PyLong_AsUnsignedLongLong(paramValueObj);
        break;
    case TYPE_FLT32:
        paramValue.val_flt32 = (flt32)PyLong_AsDouble(paramValueObj);
        break;
    case TYPE_FLT64:
        paramValue.val_flt64 = (flt64)PyLong_AsDouble(paramValueObj);
        break;
    case TYPE_BOOLEAN:
        paramValue.val_uns16 = (uns16)PyLong_AsUnsignedLong(paramValueObj);
        break;
    case TYPE_SMART_STREAM_TYPE_PTR: {
        ssItems = PopulateSsParams(paramValueObj);
        if (PyErr_Occurred())
            return NULL;
        paramValue.val_ss.entries = (uns16)ssItems.size();
        paramValue.val_ss.params = ssItems.data();
        break;
    }
    case TYPE_RGN_TYPE:
        paramValue.val_roi = PopulateRegion(paramValueObj);
        if (paramValue.val_roi.sbin == 0) // Invalid roi has all zeroes, let's check sbin only
            return PyErr_Format(PyExc_ValueError, "Failed to parse ROI members.");
        break;
    case TYPE_SMART_STREAM_TYPE: // Not used in PVCAM
    case TYPE_VOID_PTR: // Not used in PVCAM
    case TYPE_VOID_PTR_PTR: // Not used in PVCAM
    case TYPE_RGN_TYPE_PTR: // Not used in PVCAM
    case TYPE_RGN_LIST_TYPE: // Not used in PVCAM
    case TYPE_RGN_LIST_TYPE_PTR: // Not used in PVCAM
    default:
        return PyErr_Format(PyExc_RuntimeError,
                "Failed to match parameter type (%u).", (uns32)paramType);
    }
    if (PyErr_Occurred())
        return NULL;

    if (!pl_set_param(hcam, paramId, &paramValue))
        return PvcamError();

    Py_RETURN_NONE;
}

/** Checks if a specified parameter is available. */
static PyObject* pvc_check_param(PyObject* self, PyObject* args)
{
    int16 hcam;
    uns32 paramId;
    if (!PyArg_ParseTuple(args, "hI", &hcam, &paramId))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();

    return PyBool_FromLong(avail);
}

/** Sets up a live acquisition. */
static PyObject* pvc_setup_live(PyObject* self, PyObject* args)
{
    int16 hcam;
    PyObject* roiListObj;
    uns32 expTime;
    int16 expMode;
    uns32 bufferFrameCount; /* Number of frames in the acquisition buffer */
    char* streamToDiskPath; /* None, or location where to save the data */
    if (!PyArg_ParseTuple(args, "hO!IhIz", &hcam, &PyList_Type, &roiListObj,
                &expTime, &expMode, &bufferFrameCount, &streamToDiskPath))
        return ParamParseError();

    const std::vector<rgn_type> roiArray = PopulateRegions(roiListObj);
    if (roiArray.empty())
        return NULL;

    uns32 frameBytes;
    if (!pl_exp_setup_cont(hcam, (uns16)roiArray.size(), roiArray.data(),
                expMode, expTime, &frameBytes, CIRC_OVERWRITE))
        return PvcamError();

    if (!pl_cam_register_callback_ex3(hcam, PL_CALLBACK_EOF, (void*)NewFrameHandler, NULL))
        return PvcamError();

    bool metadataAvail = false;
    bool metadataEnabled = false;
    {
        rs_bool avail;
        if (!pl_get_param(hcam, PARAM_METADATA_ENABLED, ATTR_AVAIL, &avail))
            return PvcamError();
        metadataAvail = avail != FALSE;
        if (metadataAvail)
        {
            rs_bool cur;
            if (!pl_get_param(hcam, PARAM_METADATA_ENABLED, ATTR_CURRENT, &cur))
                return PvcamError();
            metadataEnabled = cur != FALSE;
        }
    }

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);

        cam->m_metadataEnabled = metadataEnabled;

        if (!cam->AllocateAcqBuffer(bufferFrameCount, frameBytes))
            return PyErr_Format(PyExc_MemoryError,
                    "Unable to allocate acquisition buffer for %u frame %u bytes each.",
                    bufferFrameCount, frameBytes);

        if (!cam->SetStreamToDisk(streamToDiskPath))
            return PyErr_Format(PyExc_MemoryError,
                    "Unable to set stream to disk to path '%s'.", streamToDiskPath);

        std::queue<Frame>().swap(cam->m_acqQueue);
        cam->m_acqQueueCapacity = bufferFrameCount;
        cam->m_acqAbort = false;
        cam->m_acqNewFrame = false;
        cam->m_isSequence = false;
    }

    return PyLong_FromUnsignedLong(frameBytes);
}

/** Sets up a sequence acquisition. */
static PyObject* pvc_setup_seq(PyObject* self, PyObject* args)
{
    int16 hcam;
    PyObject* roiListObj;
    uns32 expTime;
    int16 expMode;
    uns16 expTotal;
    if (!PyArg_ParseTuple(args, "hO!IhH", &hcam, &PyList_Type, &roiListObj,
                &expTime, &expMode, &expTotal))
        return ParamParseError();

    const std::vector<rgn_type> roiArray = PopulateRegions(roiListObj);
    if (roiArray.empty())
        return NULL;

    uns32 acqBufferBytes;
    if (!pl_exp_setup_seq(hcam, expTotal, (uns16)roiArray.size(), roiArray.data(),
                expMode, expTime, &acqBufferBytes))
        return PvcamError();
    const uns32 frameBytes = acqBufferBytes / expTotal;

    if (!pl_cam_register_callback_ex3(hcam, PL_CALLBACK_EOF, (void*)NewFrameHandler, NULL))
        return PvcamError();

    bool metadataAvail = false;
    bool metadataEnabled = false;
    {
        rs_bool avail;
        if (!pl_get_param(hcam, PARAM_METADATA_ENABLED, ATTR_AVAIL, &avail))
            return PvcamError();
        metadataAvail = avail != FALSE;
        if (metadataAvail)
        {
            rs_bool cur;
            if (!pl_get_param(hcam, PARAM_METADATA_ENABLED, ATTR_CURRENT, &cur))
                return PvcamError();
            metadataEnabled = cur != FALSE;
        }
    }

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);

        cam->m_metadataEnabled = metadataEnabled;

        if (!cam->AllocateAcqBuffer(expTotal, frameBytes))
            return PyErr_Format(PyExc_MemoryError,
                    "Unable to allocate acquisition buffer for %u frame %u bytes each.",
                    (uns32)expTotal, frameBytes);

        std::queue<Frame>().swap(cam->m_acqQueue);
        cam->m_acqQueueCapacity = expTotal;
        cam->m_acqAbort = false;
        cam->m_acqNewFrame = false;
        cam->m_isSequence = true;
    }

    return PyLong_FromUnsignedLong(frameBytes);
}

/** Starts already set up live acquisition. */
static PyObject* pvc_start_set_live(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    void* acqBuffer = NULL;
    uns32 acqBufferBytes = 0;
    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);

        cam->m_fpsFrameCnt = 0;
        cam->m_fpsLastTime = std::chrono::high_resolution_clock::now();
        cam->m_acqCbError.clear();

        acqBuffer = cam->m_acqBuffer->data;
        acqBufferBytes = (uns32)cam->m_acqBuffer->size;
    }

    if (!pl_exp_start_cont(hcam, acqBuffer, acqBufferBytes))
        return PvcamError();

    Py_RETURN_NONE;
}

/** Starts already set up sequence acquisition. */
static PyObject* pvc_start_set_seq(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    void* acqBuffer = NULL;
    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);

        cam->m_fpsFrameCnt = 0;
        cam->m_fpsLastTime = std::chrono::high_resolution_clock::now();
        cam->m_acqCbError.clear();

        acqBuffer = cam->m_acqBuffer->data;
    }

    if (!pl_exp_start_seq(hcam, acqBuffer))
        return PvcamError();

    Py_RETURN_NONE;
}

/** Starts a live acquisition. */
static PyObject* pvc_start_live(PyObject* self, PyObject* args)
{
    PyObject* frameBytesObj = pvc_setup_live(self, args);
    if (!frameBytesObj)
        return NULL;

    PyObject* arg1 = PyTuple_GetSlice(args, 0, 1); // Just hcam
    if (!arg1)
        return NULL;

    if (!pvc_start_set_live(self, arg1))
        frameBytesObj = NULL;

    Py_DECREF(arg1);
    return frameBytesObj;
}

/** Starts a sequence acquisition. */
static PyObject* pvc_start_seq(PyObject* self, PyObject* args)
{
    PyObject* frameBytesObj = pvc_setup_seq(self, args);
    if (!frameBytesObj)
        return NULL;

    PyObject* arg1 = PyTuple_GetSlice(args, 0, 1); // Just hcam
    if (!arg1)
        return NULL;

    if (!pvc_start_set_seq(self, arg1))
        frameBytesObj = NULL;

    Py_DECREF(arg1);
    return frameBytesObj;
}

/** Returns current acquisition status, works during acquisition only. */
static PyObject* pvc_check_frame_status(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    bool isSequence = false;
    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);
        isSequence = cam->m_isSequence;
    }

    int16 status;
    uns32 dummy;
    const rs_bool checkStatusResult = (isSequence)
        ? pl_exp_check_status(hcam, &status, &dummy)
        : pl_exp_check_cont_status(hcam, &status, &dummy, &dummy);
    if (!checkStatusResult)
        return PvcamError();

    switch (status)
    {
    case READOUT_NOT_ACTIVE:
        return PyUnicode_FromString("READOUT_NOT_ACTIVE");
    case EXPOSURE_IN_PROGRESS:
        return PyUnicode_FromString("EXPOSURE_IN_PROGRESS");
    case READOUT_IN_PROGRESS:
        return PyUnicode_FromString("READOUT_IN_PROGRESS");
    case FRAME_AVAILABLE:
        return (isSequence)
            ? PyUnicode_FromString("READOUT_COMPLETE")
            : PyUnicode_FromString("FRAME_AVAILABLE");
    case READOUT_FAILED:
        return PyUnicode_FromString("READOUT_FAILED");
    default:
        return PyErr_Format(PyExc_ValueError,
                "Unrecognized frame status (%d).", (int32)status);
    }
}

static PyObject* pvc_get_frame(PyObject* self, PyObject* args)
{
    int16 hcam;
    PyObject* roiListObj;
    int typenum; // Numpy typenum specifying data type for image data
    int timeoutMs; // Poll frame timeout in ms, negative values will wait forever
    int oldestFrameInt; // Must be int, "p" format for bool breaks other args
    if (!PyArg_ParseTuple(args, "hO!iii", &hcam, &PyList_Type, &roiListObj,
                &typenum, &timeoutMs, &oldestFrameInt))
        return ParamParseError();
    bool oldestFrame = (bool)oldestFrameInt;

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    bool isSequence = false;
    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);
        isSequence = cam->m_isSequence;
    }

    int16 status;
    uns32 dummy;
    rs_bool checkStatusResult = (isSequence)
        ? pl_exp_check_status(hcam, &status, &dummy)
        : pl_exp_check_cont_status(hcam, &status, &dummy, &dummy);

    std::unique_lock<std::mutex> lock(cam->m_mutex);

    if (timeoutMs != 0)
    {
        // Wait for a new frame, but every so often check if readout failed
        constexpr auto timeStep = std::chrono::milliseconds(5000);
        const auto timeEnd = std::chrono::high_resolution_clock::now()
            + ((timeoutMs > 0)
                ? std::chrono::milliseconds(timeoutMs)
                : std::chrono::hours(24 * 365 * 100)); // WAIT_FOREVER ~ 100 years

        // Release the GIL to allow other Python threads to run
        Py_BEGIN_ALLOW_THREADS

        // Exit loop if readout finished or failed, new data available,
        // abort occurred or timeout expired
        while (checkStatusResult && !cam->m_acqNewFrame && !cam->m_acqAbort
                && cam->m_acqCbError.empty()
                && status != READOUT_FAILED && status != READOUT_NOT_ACTIVE)
        {
            const auto now = std::chrono::high_resolution_clock::now();
            if (now >= timeEnd)
                break;

            auto timeNext = (std::min)(now + timeStep, timeEnd);
            cam->m_acqCond.wait_until(lock, timeNext);

            lock.unlock();

            checkStatusResult = (isSequence)
                ? pl_exp_check_status(hcam, &status, &dummy)
                : pl_exp_check_cont_status(hcam, &status, &dummy, &dummy);

            lock.lock();
        }

        Py_END_ALLOW_THREADS
    }

    if (!checkStatusResult)
    {
        cam->m_acqNewFrame = false;
        return PvcamError();
    }
    if (status == READOUT_FAILED)
    {
        cam->m_acqNewFrame = false;
        return PyErr_Format(PyExc_RuntimeError, "Frame readout failed.");
    }
    if (status == READOUT_NOT_ACTIVE)
    {
        cam->m_acqNewFrame = false;
        return PyErr_Format(PyExc_RuntimeError, "Acquisition not active.");
    }
    if (!cam->m_acqCbError.empty())
    {
        cam->m_acqNewFrame = false;
        return PyErr_Format(PyExc_RuntimeError, "%s", cam->m_acqCbError.c_str());
    }
    if (cam->m_acqAbort)
    {
        cam->m_acqAbort = false;
        cam->m_acqNewFrame = false;
        return PyErr_Format(PyExc_RuntimeError, "Acquisition aborted.");
    }
    if (!cam->m_acqNewFrame)
    {
        cam->m_acqAbort = false;
        return PyErr_Format(PyExc_RuntimeError, "Frame timeout."
                " Verify the timeout exceeds the exposure time."
                " If applicable, check external trigger source.");
    }

    Frame frame;
    if (oldestFrame)
    {
        frame = cam->m_acqQueue.front();
        cam->m_acqQueue.pop();
    }
    else
    {
        frame = cam->m_acqQueue.back();
    }

    cam->m_acqNewFrame = !cam->m_acqQueue.empty();

    //printf("New Data - FPS: %.1f, Cnt: %u, Nr: %u\n",
    //        cam->m_fps, frame.count, frame.nr);

    // Build Python object for new frame

    auto GetNewPyArrayRoiData = [typenum](const rgn_type& roi, void* data,
                                          std::shared_ptr<AcqBuffer> acqBuffer) -> PyObject*
    {
        // Make heap copy of shared_ptr
        auto* owner = new(std::nothrow) std::shared_ptr<AcqBuffer>(acqBuffer);
        if (!owner)
            return PyErr_Format(PyExc_MemoryError, "Unable to allocate capsule owner");

        npy_intp w = (roi.s2 - roi.s1 + 1) / roi.sbin;
        npy_intp h = (roi.p2 - roi.p1 + 1) / roi.pbin;
        constexpr int NUM_DIMS = 2;
        npy_intp dims[NUM_DIMS] = { h, w };
        PyObject* pyArray = PyArray_SimpleNewFromData(NUM_DIMS, dims, typenum, data);
        if (!pyArray)
        {
            delete owner;
            return NULL;
        }

        static constexpr const char* CAPSULE_NAME = "pvc.AcqBuffer";

        auto capsuleDtor = [](PyObject* capsule)
        {
            void* ptr = PyCapsule_GetPointer(capsule, CAPSULE_NAME);
            if (ptr)
            {
                auto* sp = reinterpret_cast<std::shared_ptr<AcqBuffer>*>(ptr);
                delete sp; // Drops refcount, maybe frees buffer if last
            }
        };

        PyObject* capsule = PyCapsule_New((void*)owner, CAPSULE_NAME, capsuleDtor);
        if (!capsule)
        {
            Py_DECREF(pyArray);
            delete owner;
            return NULL;
        }

        if (PyArray_SetBaseObject((PyArrayObject*)pyArray, capsule) < 0)
        {
            Py_DECREF(capsule); // Calls owner's destructor, drops refcount
            Py_DECREF(pyArray);
            return NULL;
        }

        return pyArray;
    };
    auto GetNewPyDictRoiHdr = [](const md_frame_roi_header* pRoiHdr) -> PyObject*
    {
        PyObject* pyRoi = Py_BuildValue("{s:H,s:H,s:H,s:H,s:H,s:H}", // dict
                "s1",   pRoiHdr->roi.s1,
                "s2",   pRoiHdr->roi.s2,
                "sbin", pRoiHdr->roi.sbin,
                "p1",   pRoiHdr->roi.p1,
                "p2",   pRoiHdr->roi.p2,
                "pbin", pRoiHdr->roi.pbin);
        if (!pyRoi)
            return NULL;

        PyObject* pyDict = Py_BuildValue("{s:H,s:I,s:I,s:N,s:B,s:H,s:I}", // dict
                "roiNr", pRoiHdr->roiNr,
                "timestampBOR", pRoiHdr->timestampBOR,
                "timestampEOR", pRoiHdr->timestampEOR,
                "roi", pyRoi,
                "flags", pRoiHdr->flags,
                "extendedMdSize", pRoiHdr->extendedMdSize,
                "roiDataSize", pRoiHdr->roiDataSize);
        if (!pyDict)
        {
            Py_DECREF(pyRoi);
            return NULL;
        }
        return pyDict;
    };
    auto GetNewPyDictFrameHdr = [](const md_frame_header* pFrameHdr) -> PyObject*
    {
        ulong64 timestampBofPs;
        ulong64 timestampEofPs;
        ulong64 exposureTimePs;
        if (pFrameHdr->version >= 3)
        {
            auto pFrameHdrV3 = reinterpret_cast<const md_frame_header_v3*>(pFrameHdr);
            timestampBofPs = pFrameHdrV3->timestampBOF;
            timestampEofPs = pFrameHdrV3->timestampEOF;
            exposureTimePs = pFrameHdrV3->exposureTime;
        }
        else
        {
            timestampBofPs = 1000ULL * pFrameHdr->timestampResNs    * pFrameHdr->timestampBOF;
            timestampEofPs = 1000ULL * pFrameHdr->timestampResNs    * pFrameHdr->timestampEOF;
            exposureTimePs = 1000ULL * pFrameHdr->exposureTimeResNs * pFrameHdr->exposureTime;
        }

        uns8 imageFormat;
        uns8 imageCompression;
        if (pFrameHdr->version >= 2)
        {
            imageFormat = pFrameHdr->imageFormat;
            imageCompression = pFrameHdr->imageCompression;
        }
        else
        {
            imageFormat = (uns8)PL_IMAGE_FORMAT_MONO16;
            imageCompression = (uns8)PL_IMAGE_COMPRESSION_NONE;
        }

        PyObject* pyDict = Py_BuildValue(
                "{s:s,s:B,s:I,s:H,s:K,s:K,s:K,s:B,s:B,s:B,s:H,s:B,s:B}", // dict
                "signature", reinterpret_cast<const char*>(&pFrameHdr->signature),
                "version", pFrameHdr->version,
                "frameNr", pFrameHdr->frameNr,
                "roiCount", pFrameHdr->roiCount,
                "timestampBofPs", timestampBofPs,
                "timestampEofPs", timestampEofPs,
                "exposureTimePs", exposureTimePs,
                "bitDepth", pFrameHdr->bitDepth,
                "colorMask", pFrameHdr->colorMask,
                "flags", pFrameHdr->flags,
                "extendedMdSize", pFrameHdr->extendedMdSize,
                "imageFormat", imageFormat,
                "imageCompression", imageCompression);
        return pyDict;
    };

    // Ensure the typenum is valid Numpy type
    PyArray_Descr* descr = PyArray_DescrFromType(typenum);
    if (!descr)
        return PyErr_Format(PyExc_ValueError, "Invalid NumPy type number: %d", typenum);
    Py_DECREF(descr);

    PyObject* pyFrameDict = PyDict_New();
    if (!pyFrameDict)
        return NULL;
    PyObject* pyRoiDataList = NULL;

    if (cam->m_metadataEnabled)
    {
        md_frame* mdFrame = cam->m_mdFrame;
        uns32 frameBytes = cam->m_frameBytes;

        lock.unlock();

        const rs_bool frameDecodeResult =
            pl_md_frame_decode(mdFrame, frame.address, frameBytes);

        lock.lock();

        if (!frameDecodeResult)
        {
            Py_DECREF(pyFrameDict);
            return PvcamError();
        }

        const md_frame_header* pFrameHdr = cam->m_mdFrame->header;

        PyObject* pyFrameHdrDict = GetNewPyDictFrameHdr(pFrameHdr);
        if (!pyFrameHdrDict)
        {
            Py_DECREF(pyFrameDict);
            return NULL;
        }

        PyObject* pyRoiHdrList = PyList_New(pFrameHdr->roiCount);
        if (!pyRoiHdrList)
        {
            Py_DECREF(pyFrameHdrDict);
            Py_DECREF(pyFrameDict);
            return NULL;
        }

        pyRoiDataList = PyList_New(pFrameHdr->roiCount);
        if (!pyRoiDataList)
        {
            Py_DECREF(pyRoiHdrList);
            Py_DECREF(pyFrameHdrDict);
            Py_DECREF(pyFrameDict);
            return NULL;
        }

        for (uns32 i = 0; i < pFrameHdr->roiCount; i++)
        {
            const md_frame_roi_header* pRoiHdr = cam->m_mdFrame->roiArray[i].header;
            void* pRoiData = cam->m_mdFrame->roiArray[i].data;

            PyObject* pyRoiHdr = GetNewPyDictRoiHdr(pRoiHdr);
            if (!pyRoiHdr)
            {
                Py_DECREF(pyRoiDataList);
                Py_DECREF(pyRoiHdrList);
                Py_DECREF(pyFrameHdrDict);
                Py_DECREF(pyFrameDict);
                return NULL;
            }
            PyList_SET_ITEM(pyRoiHdrList, (Py_ssize_t)i, pyRoiHdr);

            PyObject* pyRoiData =
                GetNewPyArrayRoiData(pRoiHdr->roi, pRoiData, cam->m_acqBuffer);
            if (!pyRoiData)
            {
                Py_DECREF(pyRoiDataList);
                Py_DECREF(pyRoiHdrList);
                Py_DECREF(pyFrameHdrDict);
                Py_DECREF(pyFrameDict);
                return NULL;
            }
            PyList_SET_ITEM(pyRoiDataList, (Py_ssize_t)i, pyRoiData);
        }

        PyObject* pyMetaDict = Py_BuildValue("{s:N,s:N}", // dict
                "frame_header", pyFrameHdrDict,
                "roi_headers", pyRoiHdrList);
        if (!pyMetaDict)
        {
            Py_DECREF(pyRoiDataList);
            Py_DECREF(pyRoiHdrList);
            Py_DECREF(pyFrameHdrDict);
            Py_DECREF(pyFrameDict);
            return NULL;
        }

        if (PyDict_SetItemString(pyFrameDict, "meta_data", pyMetaDict) < 0)
        {
            Py_DECREF(pyMetaDict);
            Py_DECREF(pyRoiDataList);
            Py_DECREF(pyFrameDict);
            return NULL;
        }
        Py_DECREF(pyMetaDict);
    }
    else
    {
        // Construct ROI list with 1 region only, because metadata is disabled
        const std::vector<rgn_type> rois = PopulateRegions(roiListObj);
        if (rois.empty())
        {
            Py_DECREF(pyFrameDict);
            return NULL;
        }

        pyRoiDataList = PyList_New(1);
        if (!pyRoiDataList)
        {
            Py_DECREF(pyFrameDict);
            return NULL;
        }

        PyObject* pyRoiData =
            GetNewPyArrayRoiData(rois[0], frame.address, cam->m_acqBuffer);
        if (!pyRoiData)
        {
            Py_DECREF(pyRoiDataList);
            Py_DECREF(pyFrameDict);
            return NULL;
        }
        PyList_SET_ITEM(pyRoiDataList, 0, pyRoiData);
    }

    if (PyDict_SetItemString(pyFrameDict, "pixel_data", pyRoiDataList) < 0)
    {
        Py_DECREF(pyRoiDataList);
        Py_DECREF(pyFrameDict);
        return NULL;
    }
    Py_DECREF(pyRoiDataList);

    // Create final tuple (takes ownership of pyFrameDict)
    PyObject* pyResultTuple = Py_BuildValue("NdI", pyFrameDict, cam->m_fps, frame.count);
    if (!pyResultTuple)
    {
        Py_DECREF(pyFrameDict);
        return NULL;
    }

    return pyResultTuple;
}

static PyObject* pvc_finish_seq(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    void* acqBuffer = NULL;
    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);
        acqBuffer = cam->m_acqBuffer->data;
    }

    // Also internally aborts the acquisition if necessary
    if (!pl_exp_finish_seq(hcam, acqBuffer, 0))
        return PvcamError();

    if (!pl_cam_deregister_callback(hcam, PL_CALLBACK_EOF))
        return PvcamError();

    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);

        cam->m_acqAbort = true;

        cam->UnsetStreamToDisk();

        cam->m_acqCond.notify_all(); // Wakeup get_frame if anybody waits
    }

    Py_RETURN_NONE;
}

static PyObject* pvc_abort(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    if (!pl_exp_abort(hcam, CCS_HALT))
        return PvcamError();

    if (!pl_cam_deregister_callback(hcam, PL_CALLBACK_EOF))
        return PvcamError();

    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);

        cam->m_acqAbort = true;

        cam->UnsetStreamToDisk();

        cam->m_acqCond.notify_all(); // Wakeup get_frame if anybody waits
    }

    Py_RETURN_NONE;
}

// TODO: Deprecated, remove in next major release.
static PyObject* pvc_stop_live(PyObject* self, PyObject* args)
{
    return pvc_abort(self, args);
}

static PyObject* pvc_reset_frame_counter(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    std::shared_ptr<Camera> cam = GetCamera(hcam);
    if (!cam)
        return NULL;

    {
        std::lock_guard<std::mutex> lock(cam->m_mutex);
        cam->m_acqFrameCnt = 0;
    }

    Py_RETURN_NONE;
}

/**
 * Used to set the exposure out mode of a camera.
 *
 * Since exp_mode and exp_out_mode are a read only parameters, the only way
 * to change it is by setting up an acquisition and providing the desired
 * exposure out mode there.
 */
static PyObject* pvc_set_exp_modes(PyObject* self, PyObject* args)
{
    int16 hcam;
    int16 expMode; // The bit-wise OR between exposure mode and expose out mode
    if (!PyArg_ParseTuple(args, "hh", &hcam, &expMode))
        return ParamParseError();

    // Struct that contains the frame size and binning information.
    rgn_type roi = {0, 1, 1, 0, 1, 1};
    uns32 exposureBytes;
    if (!pl_exp_setup_seq(hcam, 1, 1, &roi, expMode, 0, &exposureBytes))
        return PvcamError();

    Py_RETURN_NONE;
}

static PyObject* pvc_read_enum(PyObject* self, PyObject* args)
{
    int16 hcam;
    uns32 paramId;
    if (!PyArg_ParseTuple(args, "hI", &hcam, &paramId))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();
    if (!avail)
        return PyErr_Format(PyExc_AttributeError,
                "Invalid setting for this camera. Parameter ID 0x%08X is not available.",
                paramId);

    uns32 count;
    if (!pl_get_param(hcam, paramId, ATTR_COUNT, &count))
        return PvcamError();

    std::vector<std::pair<std::vector<char>, int32>> items(count);
    for (uns32 i = 0; i < count; i++)
    {
        uns32 strLen;
        if (!pl_enum_str_length(hcam, paramId, i, &strLen))
            return PvcamError();

        std::vector<char> strBuf(strLen);
        int32 value;
        if (!pl_get_enum_param(hcam, paramId, i, &value, strBuf.data(), strLen))
            return PvcamError();

        items[i].first = strBuf;
        items[i].second = value;
    }

    PyObject* pyResultDict = PyDict_New();
    if (!pyResultDict)
        return NULL;
    for (uns32 i = 0; i < count; i++)
    {
        const auto& item = items[i];
        const char* name = item.first.data();
        PyObject* pyValue = PyLong_FromLong(item.second);
        if (!pyValue)
        {
            Py_DECREF(pyResultDict);
            return NULL;
        }

        if (PyDict_SetItemString(pyResultDict, name, pyValue) < 0)
        {
            Py_DECREF(pyValue);
            Py_DECREF(pyResultDict);
            return NULL;
        }
        Py_DECREF(pyValue);
    }
    return pyResultDict;
}

/** Resets all prost-processing features of the open camera. */
static PyObject* pvc_reset_pp(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    if (!pl_pp_reset(hcam))
        return PvcamError();

    Py_RETURN_NONE;
}

/** Sends a software trigger to the camera */
static PyObject* pvc_sw_trigger(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    uns32 flags = 0;
    uns32 value = 0;
    if (!pl_exp_trigger(hcam, &flags, value))
        //return PvcamError();
        // TODO: This should be rather RuntimeError
        return PyErr_Format(PyExc_ValueError, "Failed to deliver software trigger.");

    if (flags != PL_SW_TRIG_STATUS_TRIGGERED)
        // TODO: This should be rather RuntimeError
        return PyErr_Format(PyExc_ValueError, "Failed to perform software trigger.");

    Py_RETURN_NONE;
}

// Module definition

#define PVC_ADD_METHOD_(name, args, docstring) { #name, pvc_##name, args, PyDoc_STR(docstring) }
static PyMethodDef pvcMethods[] = {

    PVC_ADD_METHOD_(init_pvcam, METH_NOARGS,
            "Initializes PVCAM library."),
    PVC_ADD_METHOD_(uninit_pvcam, METH_NOARGS,
            "Uninitializes PVCAM library."),
    PVC_ADD_METHOD_(get_pvcam_version, METH_NOARGS,
            "Returns the current version of PVCAM."),
    PVC_ADD_METHOD_(get_cam_fw_version, METH_VARARGS,
            "Gets the camera firmware version."),

    PVC_ADD_METHOD_(get_cam_total, METH_NOARGS,
            "Returns the number of available cameras currently connected to the system."),
    PVC_ADD_METHOD_(get_cam_name, METH_VARARGS,
            "Return the name of a camera given its handle/camera number."),
    PVC_ADD_METHOD_(open_camera, METH_VARARGS,
            "Opens a specified camera."),
    PVC_ADD_METHOD_(close_camera, METH_VARARGS,
            "Closes a specified camera."),

    PVC_ADD_METHOD_(get_param, METH_VARARGS,
            "Returns the value of a camera associated with the specified parameter."),
    PVC_ADD_METHOD_(set_param, METH_VARARGS,
            "Sets a specified parameter to a specified value."),
    PVC_ADD_METHOD_(check_param, METH_VARARGS,
            "Checks if a specified setting of a camera is available."),

    PVC_ADD_METHOD_(setup_live, METH_VARARGS,
            "Sets up live mode acquisition."),
    PVC_ADD_METHOD_(setup_seq, METH_VARARGS,
            "Sets up sequence mode acquisition."),
    PVC_ADD_METHOD_(start_set_live, METH_VARARGS,
            "Starts already set up live mode acquisition."),
    PVC_ADD_METHOD_(start_set_seq, METH_VARARGS,
            "Starts already set up sequence mode acquisition."),
    PVC_ADD_METHOD_(start_live, METH_VARARGS,
            "Sets up and starts live mode acquisition."),
    PVC_ADD_METHOD_(start_seq, METH_VARARGS,
            "Sets up and starts sequence mode acquisition."),
    PVC_ADD_METHOD_(check_frame_status, METH_VARARGS,
            "Checks status of frame transfer."),
    PVC_ADD_METHOD_(get_frame, METH_VARARGS,
            "Gets oldest or latest frame."),
    PVC_ADD_METHOD_(finish_seq, METH_VARARGS,
            "Finishes sequence mode acquisition. Must be called before another start_seq with different configuration."),
    PVC_ADD_METHOD_(abort, METH_VARARGS,
            "Aborts acquisition."),
    PVC_ADD_METHOD_(stop_live, METH_VARARGS,
            "Aborts/stops acquisition. Deprecated, use 'abort' function."),

    PVC_ADD_METHOD_(reset_frame_counter, METH_VARARGS,
            "Resets a frame counter returned by get_frame."),
    PVC_ADD_METHOD_(set_exp_modes, METH_VARARGS,
            "Sets a camera's exposure mode or expose out mode."),
    PVC_ADD_METHOD_(read_enum, METH_VARARGS,
            "Returns a list of all key-value pairs of a given enum type."),
    PVC_ADD_METHOD_(reset_pp, METH_VARARGS,
            "Resets all post-processing modules to their default values."),
    PVC_ADD_METHOD_(sw_trigger, METH_VARARGS,
            "Triggers exposure using current camera settings."),

    { NULL, NULL, 0, NULL }
};
#undef PVC_ADD_METHOD_

static struct PyModuleDef pvcModule = {
    PyModuleDef_HEAD_INIT,
    "pvc", // Name of module
    "Provides an interface to PVCAM cameras.", // Module documentation
    -1, // Module keeps state in global variables
    pvcMethods // Module functions
};

PyMODINIT_FUNC
PyInit_pvc(void)
{
    import_array();  // Import numpy API (includes 'return NULL;' on error)

    return PyModule_Create(&pvcModule);
}
