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

static constexpr int NUM_DIMS = 2;
static constexpr uns16 MAX_ROIS = 15;
static constexpr uns32 ALIGNMENT_BOUNDARY = 4096;

// Local types

struct Frame_T
{
    void* address;
    uns32 count;
};

typedef union Param_Val_T
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
}
Param_Val_T;

class Camera
{
public:
    Camera()
    {
    }

    ~Camera()
    {
        UnsetStreamToDisk();
        ReleaseAcqBuffer();
    }

    void ResetQueue()
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        std::queue<Frame_T>().swap(m_frameQueue);
    }

    bool AllocateAcqBuffer(uns32 frameCount, uns32 frameBytes)
    {
        ReleaseAcqBuffer();

        const uint64_t bufferBytes64 = (uint64_t)frameBytes * frameCount;
        // PVCAM supports buffer up to 4GB only
        if (bufferBytes64 > (std::numeric_limits<uns32>::max)())
            return false;
        const uns32 bufferBytes = (uns32)bufferBytes64;

        // Always align frameBuffer on a page boundary.
        // This is required for non-buffered streaming to disk.
#ifdef _WIN32
        m_acqBuffer = _aligned_malloc(bufferBytes, ALIGNMENT_BOUNDARY);
#else
        m_acqBuffer = aligned_alloc(ALIGNMENT_BOUNDARY, bufferBytes);
#endif
        if (!m_acqBuffer)
            return false;

        m_acqBufferBytes = bufferBytes;
        m_frameCount = frameCount;
        m_frameBytes = frameBytes;

        return true;
    }

    void ReleaseAcqBuffer()
    {
#ifdef _WIN32
        _aligned_free(m_acqBuffer);
#else
        free(m_acqBuffer);
#endif
        m_acqBuffer = NULL;
        m_acqBufferBytes = 0;
        m_frameCount = 0;
        m_frameBytes = 0;
    }

    void InitializeMetadata()
    {
        memset(&m_mdFrame, 0, sizeof(m_mdFrame));
        memset(&m_mdFrameRoiArray, 0, sizeof(m_mdFrameRoiArray));

        m_mdFrame.roiArray = m_mdFrameRoiArray;
        m_mdFrame.roiCapacity = MAX_ROIS;
    }

    bool SetStreamToDisk(const char* streamToDiskPath)
    {
        if (!streamToDiskPath)
            return true;

        if (m_streamFileHandle != cInvalidFileHandle)
            return false; // Already streaming

        printf("Stream to disk path set: '%s'\n", streamToDiskPath);

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

    void StreamFrameToDisk(void* frameAddress)
    {
        if (m_streamFileHandle == cInvalidFileHandle)
            return;

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
        const uintptr_t availableIndex = (uintptr_t)frameAddress - (uintptr_t)m_acqBuffer;
        if (availableIndex > m_readIndex)
        {
            if (availableBytes < availableIndex - m_readIndex)
            {
                availableBytes = (uns32)(availableIndex - m_readIndex);
            }
        }

        uns32 bytesToWrite = (availableBytes / ALIGNMENT_BOUNDARY) * ALIGNMENT_BOUNDARY;
        const bool lastFrameInBuffer =
            (m_acqBufferBytes - (m_readIndex + bytesToWrite)) < ALIGNMENT_BOUNDARY;
        if (lastFrameInBuffer)
        {
            bytesToWrite += ALIGNMENT_BOUNDARY;
        }

        void* alignedFrameData = &(reinterpret_cast<uns8*>(m_acqBuffer)[m_readIndex]);
        uns32 bytesWritten = 0;
#ifdef _WIN32
        ::WriteFile(m_streamFileHandle, alignedFrameData, (DWORD)bytesToWrite,
                (LPDWORD)&bytesWritten, NULL);
#else
        bytesWritten += ::write(m_streamFileHandle, alignedFrameData, bytesToWrite);
#endif
        if (bytesWritten != bytesToWrite)
        {
            printf("Stream to disk error: Not all bytes written."
                    " Corrupted frames written to disk. Expected: %u Written: %u\n",
                    bytesToWrite, bytesWritten);
        }

        // Store the count of frame bytes not written.
        // Increment read index or reset to start of frame buffer if needed
        m_frameResidual = (lastFrameInBuffer) ? 0 : availableBytes - bytesWritten;
        m_readIndex = (lastFrameInBuffer) ? 0 : m_readIndex + bytesWritten;
    }

    void UnsetStreamToDisk()
    {
        if (m_streamFileHandle == cInvalidFileHandle)
            return;

        if (m_frameResidual != 0)
        {
            void* alignedFrameData = &(reinterpret_cast<uns8*>(m_acqBuffer)[m_readIndex]);
            uns32 bytesWritten = 0;
#ifdef _WIN32
            ::WriteFile(m_streamFileHandle, alignedFrameData, (DWORD)ALIGNMENT_BOUNDARY,
                    (LPDWORD)&bytesWritten, NULL);
#else
            bytesWritten += ::write(m_streamFileHandle, alignedFrameData, ALIGNMENT_BOUNDARY);
#endif
            if (bytesWritten != ALIGNMENT_BOUNDARY)
            {
                printf("Stream to disk error: Not all bytes written."
                        " Corrupted frames written to disk. Expected: %u Written: %u\n",
                        ALIGNMENT_BOUNDARY, bytesWritten);
            }
        }

#ifdef _WIN32
        ::CloseHandle(m_streamFileHandle);
#else
        ::close(m_streamFileHandle);
#endif
        m_streamFileHandle = cInvalidFileHandle;
    }

    void* m_acqBuffer{ NULL }; // Address of all frames
    uns32 m_acqBufferBytes{ 0 };
    uns32 m_frameCount{ 0 };
    uns32 m_frameBytes{ 0 };

    std::chrono::time_point<std::chrono::high_resolution_clock> m_prevFpsTime{
        std::chrono::high_resolution_clock::now() };
    double m_fps{ 0.0 };
    uns32 m_frameCnt{ 0 };

    bool m_abortAcq{ false };
    bool m_newData{ false };
    bool m_isSequence{ false };

    std::mutex m_frameMutex{};
    std::condition_variable m_frameCond{};
    std::queue<Frame_T> m_frameQueue{};

    // Meta data objects
    bool m_metadataAvail{ false };
    bool m_metadataEnabled{ false };
    // TODO: Allocate dynamically in camera open or acq. setup
    md_frame m_mdFrame{};
    // TODO: Allocate dynamically in camera open or acq. setup
    md_frame_roi m_mdFrameRoiArray[MAX_ROIS]{};

    // Stream to disk
    FileHandle m_streamFileHandle{ cInvalidFileHandle };
    uns32 m_readIndex{ 0 }; // Position in m_acqBuffer to save data from
    uns32 m_frameResidual{ 0 };
};

// Global variables

std::map<int16, Camera> g_camInstanceMap{}; // The key is hcam camera handle
std::mutex              g_camInstanceMutex{};

// Local functions

/** Helper that always returns NULL and raises ValueError "Invalid parameters." message. */
static PyObject* ParamParseError()
{
    PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
    return NULL;
}

/** Helper that always returns NULL and raises RuntimeError with PVCAM error message. */
static PyObject* PvcamError()
{
    char errMsg[ERROR_MSG_LEN] = "<UNKNOWN ERROR>";
    pl_error_message(pl_error_code(), errMsg); // Ignore PVCAM error
    PyErr_SetString(PyExc_RuntimeError, errMsg);
    return NULL;
}

static void PopulateMetadataFrameHeader(const md_frame_header* pFrameHdr, PyObject* hdrObj)
{
    PyDict_SetItem(hdrObj, PyUnicode_FromString("signature"), PyUnicode_FromString(reinterpret_cast<const char*>(&pFrameHdr->signature)));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("version"), PyLong_FromLong(pFrameHdr->version));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("frameNr"), PyLong_FromLong(pFrameHdr->frameNr));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("roiCount"), PyLong_FromLong(pFrameHdr->roiCount));

    if (pFrameHdr->version >= 3)
    {
        auto pFrameHdrV3 = reinterpret_cast<const md_frame_header_v3*>(pFrameHdr);
        PyDict_SetItem(hdrObj, PyUnicode_FromString("timestampBOF"), PyLong_FromUnsignedLongLong(pFrameHdrV3->timestampBOF));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("timestampEOF"), PyLong_FromUnsignedLongLong(pFrameHdrV3->timestampEOF));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("exposureTime"), PyLong_FromUnsignedLongLong(pFrameHdrV3->exposureTime));
    }
    else
    {
        PyDict_SetItem(hdrObj, PyUnicode_FromString("timestampBOF"), PyLong_FromLong(pFrameHdr->timestampBOF));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("timestampEOF"), PyLong_FromLong(pFrameHdr->timestampEOF));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("timestampResNs"), PyLong_FromLong(pFrameHdr->timestampResNs));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("exposureTime"), PyLong_FromLong(pFrameHdr->exposureTime));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("exposureTimeResNs"), PyLong_FromLong(pFrameHdr->exposureTimeResNs));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("roiTimestampResNs"), PyLong_FromLong(pFrameHdr->roiTimestampResNs));
    }

    PyDict_SetItem(hdrObj, PyUnicode_FromString("bitDepth"), PyLong_FromLong(pFrameHdr->bitDepth));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("colorMask"), PyLong_FromLong(pFrameHdr->colorMask));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("flags"), PyLong_FromLong(pFrameHdr->flags));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("extendedMdSize"), PyLong_FromLong(pFrameHdr->extendedMdSize));

    if (pFrameHdr->version >= 2)
    {
        PyDict_SetItem(hdrObj, PyUnicode_FromString("imageFormat"), PyLong_FromLong(pFrameHdr->imageFormat));
        PyDict_SetItem(hdrObj, PyUnicode_FromString("imageCompression"), PyLong_FromLong(pFrameHdr->imageCompression));
    }
}

static void PopulateMetadataRoiHeader(const md_frame_roi_header* pRoiHdr, PyObject* hdrObj)
{
    PyDict_SetItem(hdrObj, PyUnicode_FromString("roiNr"), PyLong_FromLong(pRoiHdr->roiNr));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("timestampBOR"), PyLong_FromLong(pRoiHdr->timestampBOR));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("timestampEOR"), PyLong_FromLong(pRoiHdr->timestampEOR));

    PyObject* roi = PyDict_New();
    PyDict_SetItem(roi, PyUnicode_FromString("s1"), PyLong_FromLong(pRoiHdr->roi.s1));
    PyDict_SetItem(roi, PyUnicode_FromString("s2"), PyLong_FromLong(pRoiHdr->roi.s2));
    PyDict_SetItem(roi, PyUnicode_FromString("sbin"), PyLong_FromLong(pRoiHdr->roi.sbin));
    PyDict_SetItem(roi, PyUnicode_FromString("p1"), PyLong_FromLong(pRoiHdr->roi.p1));
    PyDict_SetItem(roi, PyUnicode_FromString("p2"), PyLong_FromLong(pRoiHdr->roi.p2));
    PyDict_SetItem(roi, PyUnicode_FromString("pbin"), PyLong_FromLong(pRoiHdr->roi.pbin));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("roi"), roi);

    PyDict_SetItem(hdrObj, PyUnicode_FromString("flags"), PyLong_FromLong(pRoiHdr->flags));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("extendedMdSize"), PyLong_FromLong(pRoiHdr->extendedMdSize));
    PyDict_SetItem(hdrObj, PyUnicode_FromString("roiDataSize"), PyLong_FromLong(pRoiHdr->roiDataSize));
}

static void PopulateRegions(std::vector<rgn_type>& roiArray, PyObject* roiListObj)
{
    for (size_t i = 0; i < roiArray.size(); i++)
    {
        PyObject* roiObj  = PyList_GetItem(roiListObj, (Py_ssize_t)i);

        PyObject* s1Obj   = PyObject_GetAttrString(roiObj, "s1");
        PyObject* s2Obj   = PyObject_GetAttrString(roiObj, "s2");
        PyObject* sbinObj = PyObject_GetAttrString(roiObj, "sbin");
        PyObject* p1Obj   = PyObject_GetAttrString(roiObj, "p1");
        PyObject* p2Obj   = PyObject_GetAttrString(roiObj, "p2");
        PyObject* pbinObj = PyObject_GetAttrString(roiObj, "pbin");

        roiArray[i].s1   = (uns16)PyLong_AsLong(s1Obj);
        roiArray[i].s2   = (uns16)PyLong_AsLong(s2Obj);
        roiArray[i].sbin = (uns16)PyLong_AsLong(sbinObj);
        roiArray[i].p1   = (uns16)PyLong_AsLong(p1Obj);
        roiArray[i].p2   = (uns16)PyLong_AsLong(p2Obj);
        roiArray[i].pbin = (uns16)PyLong_AsLong(pbinObj);
    }
}

static void NewFrameHandler(FRAME_INFO* pFrameInfo, void* context)
{
    try
    {
        std::lock_guard<std::mutex> lock(g_camInstanceMutex);
        Camera& cam = g_camInstanceMap.at(pFrameInfo->hCam);

        cam.m_frameCnt++;

        //printf("New frame callback. Frame count %u\n", cam.m_frameCnt);

        // Re-compute FPS every 5 frames
        constexpr int FPS_FRAME_COUNT = 5;
        if (cam.m_frameCnt % FPS_FRAME_COUNT == 0)
        {
            const auto now = std::chrono::high_resolution_clock::now();
            const auto timeDeltaUs =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    now - cam.m_prevFpsTime).count();
            if (timeDeltaUs != 0)
            {
                cam.m_fps = (double)FPS_FRAME_COUNT / timeDeltaUs * 1e6;
                cam.m_prevFpsTime = now;

                //printf("fps: %.1f timeDeltaUs: %lld\n", cam.m_fps, timeDeltaUs);
            }
        }

        Frame_T frame;
        if (!pl_exp_get_latest_frame(pFrameInfo->hCam, &frame.address))
        {
            PyErr_SetString(PyExc_ValueError, "Failed to get latest frame");
        }
        else
        {
            // Add frame to queue.
            // Reset the queue in live mode so that returned frame is the latest.
            if (!cam.m_isSequence)
            {
                cam.ResetQueue();
            }

            {
                std::lock_guard<std::mutex> lock(cam.m_frameMutex);
                frame.count = cam.m_frameCnt;
                cam.m_frameQueue.push(frame);
                cam.m_newData = true;
            }

            if (cam.m_streamFileHandle != cInvalidFileHandle)
            {
                cam.StreamFrameToDisk(frame.address);
            }

            cam.m_frameCond.notify_all();
        }
    }
    catch (const std::out_of_range& ex)
    {
        // TODO: Change to PyErr_SetString call? Will it be propagated to other threads?
        printf("New frame handler: Invalid camera instance key. %s\n", ex.what());
    }
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

    // Get default-constructed Camera
    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    Camera& cam = g_camInstanceMap[hcam];

    rs_bool avail;
    if (!pl_get_param(hcam, PARAM_METADATA_ENABLED, ATTR_AVAIL, &avail))
        return PvcamError();
    cam.m_metadataAvail = avail != FALSE;

    return PyLong_FromLong(hcam);
}

/** Closes a camera given its handle. */
static PyObject* pvc_close_camera(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    // Clear instance data
    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    g_camInstanceMap.erase(hcam);

    if (!pl_cam_close(hcam))
        return PvcamError();

    Py_RETURN_NONE;
}

/** Returns the value of the specified parameter. */
static PyObject* pvc_get_param(PyObject* self, PyObject* args)
{
    int16 hcam;
    uns32 paramId;
    int16 paramAttr;
    if (!PyArg_ParseTuple(args, "hih", &hcam, &paramId, &paramAttr))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();
    if (!avail)
    {
        PyErr_SetString(PyExc_AttributeError, "Invalid setting for this camera.");
        return NULL;
    }

    uns16 paramType;
    if (!pl_get_param(hcam, paramId, ATTR_TYPE, &paramType))
        return PvcamError();

    Param_Val_T paramValue;
    if (!pl_get_param(hcam, paramId, paramAttr, &paramValue))
        return PvcamError();

    switch(paramType)
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
        return PyLong_FromDouble(paramValue.val_flt32);
    case TYPE_FLT64:
        return PyLong_FromDouble(paramValue.val_flt64);
    case TYPE_BOOLEAN:
        if (paramValue.val_bool)
            Py_RETURN_TRUE;
        else
            Py_RETURN_FALSE;
    // TODO: Add support for missing parameter types like smart streaming or ROI
    default:
        break;
    }

    PyErr_SetString(PyExc_RuntimeError, "Failed to match parameter type");
    return NULL;
}

/** Sets a specified parameter to a given value. */
static PyObject* pvc_set_param(PyObject* self, PyObject* args)
{
    int16 hcam;
    uns32 paramId;
    // TODO: Fix! The value parsed as 32-bit integer
    void* paramValue;
    if (!PyArg_ParseTuple(args, "hii", &hcam, &paramId, &paramValue))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();
    if (!avail)
    {
        PyErr_SetString(PyExc_AttributeError, "Invalid setting for this camera.");
        return NULL;
    }

    if (!pl_set_param(hcam, paramId, &paramValue))
        return PvcamError();

    Py_RETURN_NONE;
}

/** Checks if a specified parameter is available. */
static PyObject* pvc_check_param(PyObject* self, PyObject* args)
{
    int16 hcam;
    uns32 paramId;
    if (!PyArg_ParseTuple(args, "hi", &hcam, &paramId))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();

    if (avail)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

/** Starts a live acquisition. */
static PyObject* pvc_start_live(PyObject* self, PyObject* args)
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

    if (!pl_cam_register_callback_ex3(hcam, PL_CALLBACK_EOF, NewFrameHandler, NULL))
        return PvcamError();

    /* Construct ROI list */
    const Py_ssize_t roiCount = PyList_Size(roiListObj);
    if (roiCount <= 0 || roiCount > (Py_ssize_t)(std::numeric_limits<uns16>::max)())
    {
        PyErr_SetString(PyExc_ValueError, "Invalid ROI count.");
        return NULL;
    }
    std::vector<rgn_type> roiArray(roiCount);
    PopulateRegions(roiArray, roiListObj);

    uns32 frameBytes;
    if (!pl_exp_setup_cont(hcam, (uns16)roiArray.size(), roiArray.data(),
                expMode, expTime, &frameBytes, CIRC_OVERWRITE))
        return PvcamError();

    try
    {
        std::lock_guard<std::mutex> lock(g_camInstanceMutex);
        Camera& cam = g_camInstanceMap.at(hcam);

        if (cam.m_metadataAvail)
        {
            rs_bool cur;
            if (!pl_get_param(hcam, PARAM_METADATA_ENABLED, ATTR_CURRENT, &cur))
                return PvcamError();
            cam.m_metadataEnabled = cur != FALSE;
        }

        if (!cam.AllocateAcqBuffer(bufferFrameCount, frameBytes))
        {
            PyErr_SetString(PyExc_MemoryError, "Unable to allocate acquisition.");
            return NULL;
        }

        if (!cam.SetStreamToDisk(streamToDiskPath))
        {
            PyErr_SetString(PyExc_MemoryError, "Unable to set stream to disk. Check file path.");
            return NULL;
        }

        cam.ResetQueue();
        cam.m_isSequence = false;
        cam.m_abortAcq = false;
        cam.m_newData = false;;
        cam.m_prevFpsTime = std::chrono::high_resolution_clock::now();

        if (!pl_exp_start_cont(hcam, cam.m_acqBuffer, cam.m_acqBufferBytes))
            return PvcamError();
    }
    catch (const std::out_of_range& ex)
    {
        PyErr_SetString(PyExc_KeyError, ex.what());
        return NULL;
    }

    return PyLong_FromUnsignedLong(frameBytes);
}

/** Starts a sequence acquisition. */
static PyObject* pvc_start_seq(PyObject* self, PyObject* args)
{
    int16 hcam;
    PyObject* roiListObj;
    uns32 expTime;
    int16 expMode;
    uns16 expTotal;
    if (!PyArg_ParseTuple(args, "hO!IhH", &hcam, &PyList_Type, &roiListObj,
                &expTime, &expMode, &expTotal))
        return ParamParseError();

    if (!pl_cam_register_callback_ex3(hcam, PL_CALLBACK_EOF, NewFrameHandler, NULL))
        return PvcamError();

    /* Construct ROI list */
    const Py_ssize_t roiCount = PyList_Size(roiListObj);
    if (roiCount <= 0 || roiCount > (Py_ssize_t)(std::numeric_limits<uns16>::max)())
    {
        PyErr_SetString(PyExc_ValueError, "Invalid ROI count.");
        return NULL;
    }
    std::vector<rgn_type> roiArray(roiCount);
    PopulateRegions(roiArray, roiListObj);

    uns32 acqBufferBytes;
    if (!pl_exp_setup_seq(hcam, expTotal, (uns16)roiArray.size(), roiArray.data(),
                expMode, expTime, &acqBufferBytes))
        return PvcamError();
    const uns32 frameBytes = acqBufferBytes / expTotal;

    try
    {
        std::lock_guard<std::mutex> lock(g_camInstanceMutex);
        Camera& cam = g_camInstanceMap.at(hcam);

        if (cam.m_metadataAvail)
        {
            rs_bool cur;
            if (!pl_get_param(hcam, PARAM_METADATA_ENABLED, ATTR_CURRENT, &cur))
                return PvcamError();
            cam.m_metadataEnabled = cur != FALSE;
        }

        if (!cam.AllocateAcqBuffer(expTotal, frameBytes))
        {
            PyErr_SetString(PyExc_MemoryError, "Unable to allocate acquisition buffer.");
            return NULL;
        }

        cam.ResetQueue();
        cam.m_isSequence = true;
        cam.m_abortAcq = false;
        cam.m_newData = false;;
        cam.m_prevFpsTime = std::chrono::high_resolution_clock::now();

        if (!pl_exp_start_seq(hcam, cam.m_acqBuffer))
            return PvcamError();
    }
    catch (const std::out_of_range& ex)
    {
        PyErr_SetString(PyExc_KeyError, ex.what());
        return NULL;
    }

    return PyLong_FromUnsignedLong(frameBytes);
}

/** Returns current acquisition status, works during acquisition only. */
static PyObject* pvc_check_frame_status(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    char* statusStr = "";

    try
    {
        std::lock_guard<std::mutex> lock(g_camInstanceMutex);
        Camera& cam = g_camInstanceMap.at(hcam);

        int16 status;
        uns32 dummy;
        if (cam.m_isSequence)
        {
            if (!pl_exp_check_status(hcam, &status, &dummy))
                return PvcamError();
        }
        else
        {
            if (!pl_exp_check_cont_status(hcam, &status, &dummy, &dummy))
                return PvcamError();
        }

        switch (status)
        {
        case READOUT_NOT_ACTIVE:
            statusStr = "READOUT_NOT_ACTIVE";
            break;
        case EXPOSURE_IN_PROGRESS:
            statusStr = "EXPOSURE_IN_PROGRESS";
            break;
        case READOUT_IN_PROGRESS:
            statusStr = "READOUT_IN_PROGRESS";
            break;
        case FRAME_AVAILABLE:
            statusStr = (cam.m_isSequence) ? "READOUT_COMPLETE" : "FRAME_AVAILABLE";
            break;
        case READOUT_FAILED:
            statusStr = "READOUT_FAILED";
            break;
        default:
            PyErr_SetString(PyExc_ValueError, "Unrecognized frame status.");
            return NULL;
        }
    }
    catch (const std::out_of_range& ex)
    {
        PyErr_SetString(PyExc_KeyError, ex.what());
        return NULL;
    }

    return PyUnicode_FromString(statusStr);
}

static PyObject* pvc_get_frame(PyObject* self, PyObject* args)
{
    int16 hcam;
    PyObject* roiListObj;
    int typenum; // Numpy typenum specifying data type for image data
    int timeoutMs; // Poll frame timeout in ms, negative values will wait forever
    bool oldestFrame;
    if (!PyArg_ParseTuple(args, "hO!iip", &hcam, &PyList_Type, &roiListObj,
                &typenum, &timeoutMs, &oldestFrame))
        return ParamParseError();

    // WARNING!
    // We are accessing a camera instance below without locking the object so that
    // the new frame handler or abort can take the lock and alter date. Care must be
    // taken to not alter the camera instance until the lock is obtained
    try
    {
        //std::lock_guard<std::mutex> lock(g_camInstanceMutex);
        Camera& cam = g_camInstanceMap.at(hcam);

        int16 status;
        uns32 dummy;
        rs_bool checkStatusResult = pl_exp_check_status(hcam, &status, &dummy);

        if (timeoutMs != 0)
        {
            // Wait for a new frame, but every so often check if readout failed
            constexpr auto timeStep = std::chrono::milliseconds(5000);
            const auto timeEnd = std::chrono::high_resolution_clock::now()
                + ((timeoutMs > 0)
                    ? std::chrono::milliseconds(timeoutMs)
                    : std::chrono::hours(24 * 365 * 100)); // WAIT_FOREVER ~ 100 years

            std::unique_lock<std::mutex> lock(cam.m_frameMutex);
            /* Release the GIL to allow other Python threads to run */
            /* This macro has an open brace and must be paired */
            Py_BEGIN_ALLOW_THREADS

            // Exit loop if readout finished or failed, new data available,
            // abort occurred or timeout expired
            while (checkStatusResult && !cam.m_newData && !cam.m_abortAcq
                    && (status == EXPOSURE_IN_PROGRESS || status == READOUT_IN_PROGRESS))
            {
                const auto now = std::chrono::high_resolution_clock::now();
                if (now >= timeEnd)
                    break;

                auto timeNext = (std::min)(now + timeStep, timeEnd);
                cam.m_frameCond.wait_until(lock, timeNext);

                checkStatusResult = pl_exp_check_status(hcam, &status, &dummy);
            }

            Py_END_ALLOW_THREADS
        }

        std::lock_guard<std::mutex> lock(g_camInstanceMutex);

        if (!checkStatusResult)
        {
            std::lock_guard<std::mutex> lock(cam.m_frameMutex);
            cam.m_newData = false;
            return PvcamError();
        }
        if (status == READOUT_FAILED)
        {
            std::lock_guard<std::mutex> lock(cam.m_frameMutex);
            cam.m_newData = false;
            PyErr_SetString(PyExc_RuntimeError, "Frame readout failed.");
            return NULL;
        }
        if (cam.m_abortAcq)
        {
            std::lock_guard<std::mutex> lock(cam.m_frameMutex);
            cam.m_abortAcq = false;
            cam.m_newData = false;
            PyErr_SetString(PyExc_RuntimeError, "Acquisition aborted.");
            return NULL;
        }

        Frame_T frame;
        {
            std::lock_guard<std::mutex> lock(cam.m_frameMutex);

            if (!cam.m_newData)
            {
                cam.m_abortAcq = false;
                PyErr_SetString(PyExc_RuntimeError, "Frame timeout."
                        " Verify the timeout exceeds the exposure time."
                        " If applicable, check external trigger source.");
                return NULL;
            }

            if (oldestFrame)
            {
                frame = cam.m_frameQueue.front();
                cam.m_frameQueue.pop();
            }
            else
            {
                frame = cam.m_frameQueue.back();
            }

            //printf("New Data - FPS: %.1f Cnt: %u\n", cam.m_fps, frame.count);

            // Toggle m_newData flag unless we are in sequence mode and another frame is available
            cam.m_newData = cam.m_isSequence && !cam.m_frameQueue.empty();
        }

        PyObject* frameDict = PyDict_New();
        PyObject* roiDataList = PyList_New(0);
        if (cam.m_metadataEnabled)
        {
            PyObject* frameHeader = PyDict_New();
            PyObject* roiHeaderList = PyList_New(0);
            PyObject* metaDict = PyDict_New();
            PyDict_SetItem(metaDict, PyUnicode_FromString("frame_header"), frameHeader);
            PyDict_SetItem(metaDict, PyUnicode_FromString("roi_headers"), roiHeaderList);
            PyDict_SetItem(frameDict, PyUnicode_FromString("meta_data"), metaDict);

            cam.InitializeMetadata();
            if (!pl_md_frame_decode(&cam.m_mdFrame, frame.address, cam.m_frameBytes))
                return PvcamError();

            const md_frame_header* pFrameHdr = cam.m_mdFrame.header;

            PopulateMetadataFrameHeader(pFrameHdr, frameHeader);

            for (uns32 i = 0; i < pFrameHdr->roiCount; i++)
            {
                const md_frame_roi_header* pRoiHdr = cam.m_mdFrame.roiArray[i].header;
                const rgn_type& roi = pRoiHdr->roi;

                PyObject* hdrObj = PyDict_New();
                PopulateMetadataRoiHeader(pRoiHdr, hdrObj);
                PyList_Append(roiHeaderList, hdrObj);

                npy_intp w = (roi.s2 - roi.s1 + 1) / roi.sbin;
                npy_intp h = (roi.p2 - roi.p1 + 1) / roi.pbin;
                npy_intp dims[NUM_DIMS] = { h, w };
                PyObject* numpyFrame = PyArray_SimpleNewFromData(
                        NUM_DIMS, dims, typenum, cam.m_mdFrame.roiArray[i].data);
                PyList_Append(roiDataList, numpyFrame);
            }
        }
        else
        {
            // Construct ROI list with 1 region only, because metadata is disabled
            std::vector<rgn_type> roiArray(1);
            PopulateRegions(roiArray, roiListObj);
            const rgn_type& roi = roiArray[0];

            npy_intp w = (roi.s2 - roi.s1 + 1) / roi.sbin;
            npy_intp h = (roi.p2 - roi.p1 + 1) / roi.pbin;
            npy_intp dims[NUM_DIMS] = { h, w };
            PyObject* numpyFrame = PyArray_SimpleNewFromData(
                    NUM_DIMS, dims, typenum, frame.address);
            PyList_Append(roiDataList, numpyFrame);
        }

        PyDict_SetItem(frameDict, PyUnicode_FromString("pixel_data"), roiDataList);

        PyObject* fps = PyFloat_FromDouble(cam.m_fps);
        PyObject* frameCount = PyLong_FromLong(frame.count);

        PyObject* tup = PyTuple_New(3);
        PyTuple_SetItem(tup, 0, frameDict);
        PyTuple_SetItem(tup, 1, fps);
        PyTuple_SetItem(tup, 2, frameCount);

        return tup;
    }
    catch (const std::out_of_range& ex)
    {
        PyErr_SetString(PyExc_KeyError, ex.what());
        return NULL;
    }
}

static PyObject* pvc_finish_seq(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    try
    {
        std::lock_guard<std::mutex> lock(g_camInstanceMutex);
        Camera& cam = g_camInstanceMap.at(hcam);

        // Also internally aborts the acquisition if necessary
        if (!pl_exp_finish_seq(hcam, cam.m_acqBuffer, NULL))
            return PvcamError();

        cam.m_abortAcq = true;
        cam.m_frameCond.notify_all();

        if (!pl_cam_deregister_callback(hcam, PL_CALLBACK_EOF))
            return PvcamError();

        cam.UnsetStreamToDisk();
        cam.ReleaseAcqBuffer();
}
    catch (const std::out_of_range& ex)
    {
        PyErr_SetString(PyExc_KeyError, ex.what());
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* pvc_abort(PyObject* self, PyObject* args)
{
    int16 hcam;
    if (!PyArg_ParseTuple(args, "h", &hcam))
        return ParamParseError();

    try
    {
        std::lock_guard<std::mutex> lock(g_camInstanceMutex);
        Camera& cam = g_camInstanceMap.at(hcam);

        if (!pl_exp_abort(hcam, CCS_HALT))
            return PvcamError();

        cam.m_abortAcq = true;
        cam.m_frameCond.notify_all();

        if (!pl_cam_deregister_callback(hcam, PL_CALLBACK_EOF))
            return PvcamError();

        cam.UnsetStreamToDisk();
        cam.ReleaseAcqBuffer();
    }
    catch (const std::out_of_range& ex)
    {
        PyErr_SetString(PyExc_KeyError, ex.what());
        return NULL;
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
    if (!PyArg_ParseTuple(args, "hi", &hcam, &paramId))
        return ParamParseError();

    rs_bool avail;
    if (!pl_get_param(hcam, paramId, ATTR_AVAIL, &avail))
        return PvcamError();
    if (!avail)
    {
        PyErr_SetString(PyExc_AttributeError, "Invalid setting for camera.");
        return NULL;
    }

    uns32 count;
    if (!pl_get_param(hcam, paramId, ATTR_COUNT, &count))
        return PvcamError();

    PyObject* result = PyDict_New();
    for (uns32 i = 0; i < count; i++)
    {
        uns32 strLen;
        if (!pl_enum_str_length(hcam, paramId, i, &strLen))
            return PvcamError();

        std::vector<char> strBuf(strLen);
        int32 value;
        if (!pl_get_enum_param(hcam, paramId, i, &value, strBuf.data(), strLen))
            return PvcamError();

        PyObject* pyName = PyUnicode_FromString(strBuf.data());
        PyObject* pyValue = PyLong_FromSize_t(value);
        PyDict_SetItem(result, pyName, pyValue);
    }

    return result;
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
    //    return PvcamError();
    {
        // TODO: This should be rather RuntimeError
        PyErr_SetString(PyExc_ValueError, "Failed to deliver software trigger.");
        return NULL;
    }

    if (flags != PL_SW_TRIG_STATUS_TRIGGERED)
    {
        // TODO: This should be rather RuntimeError
        PyErr_SetString(PyExc_ValueError, "Failed to perform software trigger.");
        return NULL;
    }

    Py_RETURN_NONE;
}

// Module definition

#define PVC_ADD_METHOD_(name, args, docstring) { #name, pvc_##name, args, docstring }
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

    PVC_ADD_METHOD_(start_live, METH_VARARGS,
            "Starts live mode acquisition."),
    PVC_ADD_METHOD_(start_seq, METH_VARARGS,
            "Starts sequence mode acquisition."),
    PVC_ADD_METHOD_(check_frame_status, METH_VARARGS,
            "Checks status of frame transfer."),
    PVC_ADD_METHOD_(get_frame, METH_VARARGS,
            "Gets latest frame."),
    PVC_ADD_METHOD_(finish_seq, METH_VARARGS,
            "Finishes sequence mode acquisition. Must be called before another start_seq with different configuration."),
    PVC_ADD_METHOD_(abort, METH_VARARGS,
            "Aborts acquisition."),

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
