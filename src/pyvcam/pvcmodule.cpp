#include <numpy/arrayobject.h>
#include <new>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <thread>
#include <queue>
#include "pvcmodule.h"

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

#define NPY_NO_DEPRECATED_API
static const int MAX_ROIS = 15;

struct Frame_T {
    void* address;
    uns32 count;
};

typedef union Param_Val_T {
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
} Param_Val_T;

class Cam_Instance_T {

public:
    Cam_Instance_T()
        : frameBuffer_(NULL)
        , frameCount_(0)
        , frameSize_(0)
        , prevTime_(std::chrono::high_resolution_clock::now())
        , fps_(0.0)
        , frameCnt_(0)
        , abortData_(false)
        , newData_(false)
        , seqMode_(false)
        , metaDataEnabled_(false)
        , streamToDisk_(false)
        , hFileStreamToDisk_(cInvalidFileHandle)
        , readIndex_(0)
        , frameResidual_(0)
    {
    }

    void resetQueue()
    {
        while (!frameQueue_.empty()) {
            frameQueue_.pop();
        }
    }

    void setAcquisitiontMode(bool isSequenceMode)
    {
        seqMode_ = isSequenceMode;
        abortData_ = false;
        newData_ = false;;
        resetQueue();
    }

    bool allocateFrameBuffer(uns32 sizeBytes, uns32 frameCount, uns32 frameSize)
    {
        cleanUpFrameBuffer();

        // Always align frameBuffer on a page boundary. This is required for non-buffered streaming to disk
#ifdef _WIN32
        frameBuffer_ = _aligned_malloc(sizeBytes, ALIGNMENT_BOUNDARY);
#else
        frameBuffer_ = aligned_alloc(ALIGNMENT_BOUNDARY, sizeBytes);
#endif

        frameCount_ = frameCount;
        frameSize_ = frameSize;
        return frameBuffer_ != NULL;
    }
    
    void cleanUpFrameBuffer()
    {
#ifdef _WIN32
        _aligned_free(frameBuffer_);
#else
        free(frameBuffer_);
#endif
        frameBuffer_ = NULL;
        frameCount_ = 0;
        frameSize_ = 0;
    }

    void initializeMetaData()
    {
        memset(&mdFrame_, 0, sizeof(mdFrame_));
        memset(&mdFramRoiArray_, 0, sizeof(mdFramRoiArray_));

        mdFrame_.roiArray = mdFramRoiArray_;
        mdFrame_.roiCapacity = MAX_ROIS;
    }

    bool setStreamToDisk(const char* streamToDiskPath)
    {
        bool ret = true;
        if (streamToDiskPath)
        {
            printf("Stream to disk path set: %s\n", streamToDiskPath);

            // Open file for stream to disk
#ifdef _WIN32
            const int flags = FILE_FLAG_NO_BUFFERING;
            hFileStreamToDisk_ = ::CreateFileA(streamToDiskPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, flags, NULL);
#else
            // The O_DIRECT flag is the key on Linux
            const int flags = O_DIRECT | (O_WRONLY | O_CREAT | O_TRUNC);
            const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
            hFileStreamToDisk_ = ::open(streamToDiskPath, flags, mode);
#endif
            ret = hFileStreamToDisk_ != cInvalidFileHandle;
            if (ret) {
                streamToDisk_ = true;
                readIndex_ = 0;
                frameResidual_ = 0;
            }
        }
        return ret;
    }

    void streamFrameToDisk(void* frameAddress)
    {
        if (hFileStreamToDisk_ != cInvalidFileHandle)
        {
            // When streaming to a file, we must always write to an alignment boundary. Since the frame may not be a multiple
            // of the alignment boundary, we might not write the entire frame. The remainder of the frame will be written on
            // the next pass through this function. When we reach the end of the circular buffer we may need to pad additional
            // bytes onto the end of the frame to meet the alignment requirement. These bytes are to be discarded later when
            // using the file..

            // Frames may arrive out of order. To handle this situation, readIndex_ is used to track which bytes
            // stream to file next rather than the pointer returned by PVCAM

            uns32 availableBytes = frameResidual_ + frameSize_;

            // This routine may fall behind writing the latest data. Attempt to catch up when the frameAddress is ahead of the
            // read index by more than a frame.
            uintptr_t availableIndex = (uintptr_t)frameAddress - (uintptr_t)frameBuffer_;
            if (availableIndex > readIndex_) {
                if (availableIndex - readIndex_ > frameResidual_ + frameSize_) {
                    availableBytes = availableIndex - readIndex_;
                }
            }

            uns32 bytesToWrite = (availableBytes / ALIGNMENT_BOUNDARY) * ALIGNMENT_BOUNDARY;
            bool lastFrameInBuffer = ((frameSize_ * frameCount_) - (readIndex_ + bytesToWrite)) < ALIGNMENT_BOUNDARY;
            if (lastFrameInBuffer)
            {
                bytesToWrite += ALIGNMENT_BOUNDARY;
            }

            void* alignedFrameData = &(reinterpret_cast<uns8*>(frameBuffer_)[readIndex_]);
            uns32 bytesWritten = 0;
#ifdef _WIN32
            WriteFile(hFileStreamToDisk_, alignedFrameData, (DWORD)bytesToWrite, (LPDWORD)&bytesWritten, NULL);
#else
            bytesWritten += write(hFileStreamToDisk_, alignedFrameData, bytesToWrite);
#endif
            if (bytesWritten != bytesToWrite) {
                printf("Stream to disk error: Not all bytes written. Corrupted frames written to disk. Expected: %d Written: %d\n", bytesToWrite, bytesWritten);
            }

            // Store the count of frame bytes not written.
            // Increment read index or reset to start of frame buffer if needed
            frameResidual_ = (lastFrameInBuffer) ? 0 : availableBytes - bytesWritten;
            readIndex_ = (lastFrameInBuffer) ? 0 : readIndex_ + bytesWritten;
        }
    }

    void unsetStreamToDisk()
    {
        if (hFileStreamToDisk_ != cInvalidFileHandle)
        {
#ifdef _WIN32
            CloseHandle(hFileStreamToDisk_);
#else
            close(hFileStreamToDisk_);
#endif
            streamToDisk_ = false;
            hFileStreamToDisk_ = cInvalidFileHandle;
        }
    }

    void* frameBuffer_;             /*Address of all frames*/
    uns32 frameCount_;
    uns32 frameSize_;
    std::queue<Frame_T> frameQueue_;
    std::chrono::time_point<std::chrono::high_resolution_clock> prevTime_;
    double fps_;
    uns32 frameCnt_;
    bool abortData_;
    bool newData_;
    bool seqMode_;
    std::mutex frameMutex_;
    std::condition_variable conditionalVariable_;

    // Meta data objects
    bool metaDataEnabled_;
    md_frame mdFrame_;
    md_frame_roi mdFramRoiArray_[MAX_ROIS];

    // Stream to disk
    static const size_t ALIGNMENT_BOUNDARY = 4096;
    bool streamToDisk_;
    FileHandle hFileStreamToDisk_;
    uns32 readIndex_;
    uns32 frameResidual_;
};

std::mutex g_camInstanceMutex;
std::map<int16, Cam_Instance_T> g_camInstanceMap;

// Local functions
/** Sets the global error message. */
void set_g_msg(void) { pl_error_message(pl_error_code(), g_msg); }
int is_avail(int16 hCam, uns32 param_id);
int valid_enum_param(int16 hCam, uns32 param_id, int32 selected_val);
bool check_meta_data_enabled(int16 hCam, bool& metaDataEnabled);
void populateMetaDataFrameHeader(md_frame_header* pMetaDataFrameHeader, PyObject* frame_header);
void populateMetaDataRoiHeader(md_frame_roi_header* pMetaDataRoiHeader, PyObject* roi_header);
void populateRegions(rgn_type* roiArray, uns16 numRois, PyObject * roiListObj);

void NewFrameHandler(FRAME_INFO *pFrameInfo, void *context)
{
    std::lock_guard<std::mutex> lock(g_camInstanceMutex);

    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(pFrameInfo->hCam);
        camInstance.frameCnt_++;

        //printf("Called back. Frame count %d\n", camInstance.frameCnt_);

        // Re-compute FPS every 5 frames
        const int FPS_FRAME_COUNT = 5;
        if (camInstance.frameCnt_ % FPS_FRAME_COUNT == 0){

          auto curTime = std::chrono::high_resolution_clock::now();
          auto timeDelta_us = std::chrono::duration_cast<std::chrono::microseconds>(curTime - camInstance.prevTime_).count();

          camInstance.fps_ = (double) FPS_FRAME_COUNT / (double) timeDelta_us * 1e6;
          camInstance.prevTime_ = curTime;

          //printf("fps: %lf timeDelta_us: %lld\n", g_FPS, timeDelta_us);
        }

        Frame_T frame;
        if (PV_OK != pl_exp_get_latest_frame(pFrameInfo->hCam, (void **)&frame.address)) {
          PyErr_SetString(PyExc_ValueError, "Failed to get latest frame");
        }
        else {
            // Add frame to queue. Reset the queue in live mode so that returned frame is the latest
            if (!camInstance.seqMode_) {
                camInstance.resetQueue();
            }

            frame.count = camInstance.frameCnt_;
            camInstance.frameQueue_.push(frame);
            camInstance.newData_ = true;

            if (camInstance.streamToDisk_) {
                camInstance.streamFrameToDisk(frame.address);
            }

            camInstance.conditionalVariable_.notify_all();
        }
    }
    catch (const std::out_of_range& oor) {
        std::cout << "New frame handler: Invalid camera instance key. " << oor.what() << std::endl;
    }
}

/** Returns true if the specified attribute is available. */
int is_avail(int16 hCam, uns32 param_id)
{
    rs_bool avail;
    /* Do not return falsy if a failed call to pl_get_param is made.
       Only return falsy if avail is falsy. */
    if (!pl_get_param(hCam, param_id, ATTR_AVAIL, (void *)&avail))
        return 1;
    return avail;
}

/**
  This function will return the version of the currently installed PVCAM
  library.
  @return String containing human readable PVCAM version.
*/
static PyObject *
pvc_get_pvcam_version(PyObject *self, PyObject* args)
{
    uns16 ver_num;
    if(!pl_pvcam_get_ver(&ver_num)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    uns16 major_ver_mask = 0xff00;
    uns16 minor_ver_mask = 0x00f0;
    uns16 trivial_ver_mask = 0x000f;
    char version[10];
    /*
    pl_pvcam_get_ver returns an unsigned 16 bit integer that follows the format
    MMMMMMMMrrrrTTTT where M = Major version number, r = minor version number,
    and T = trivial version number. Using bit masks and right shifts
    appropriately, create a string that has the correct human-readable version
    number. Since the max number for an 8 bit int is 255, we only need 3 chars
    to represent the major numbers, and since the max number for a 4 bit int
    is 16, we only need 4 chars across minor and trivial versions. We also
    separate versions with periods, so 2 additional chars will be needed. In
    total, we need 3 + 2 + 2 + 2 = 9 characters to represent the version number.
    */
    sprintf(version, "%d.%d.%d", (major_ver_mask & ver_num) >> 8,
                                 (minor_ver_mask & ver_num) >> 4,
                                 trivial_ver_mask & ver_num);
    return PyUnicode_FromString(version);
}

static PyObject *
pvc_get_cam_fw_version(PyObject *self, PyObject* args)
{
    int16 hCam;
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    uns16 ver_num;
    if (!pl_get_param(hCam, PARAM_CAM_FW_VERSION, ATTR_CURRENT, (void *)&ver_num)) {}

    uns16 major_ver_mask = 0xff00;
    uns16 minor_ver_mask = 0x00ff;
    char version[10];
    sprintf(version, "%d.%d", (major_ver_mask & ver_num) >> 8,
        (minor_ver_mask & ver_num));
    return PyUnicode_FromString(version);
}

/**
  This function will initialize PVCAM. Must be called before any camera
  interaction may occur.
  @return True if PVCAM successfully initialized.
*/
static PyObject *
pvc_init_pvcam(PyObject *self, PyObject* args)
{
    if(!pl_pvcam_init()) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    Py_RETURN_TRUE;
}

/**
  This function will uninitialize PVCAM.
  @return True if PVCAM successfully uninitialized.
*/
static PyObject *
pvc_uninit_pvcam(PyObject *self, PyObject* args)
{
    if (!pl_pvcam_uninit()) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    Py_RETURN_TRUE;
}

/**
  This function will return the number of available cameras currently connected
  to the system.
  @return Int containing the number of cameras available.
*/
static PyObject *
pvc_get_cam_total(PyObject *self, PyObject* args)
{
    int16 num_cams;
    if (!pl_cam_get_total(&num_cams)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    return PyLong_FromLong(num_cams);
}

/**
  This function will return a Python String containing the name of the camera
  given its handle/camera number.
  @return Name of camera.
*/
static PyObject *
pvc_get_cam_name(PyObject *self, PyObject *args)
{
    int16 cam_num;
    if (!PyArg_ParseTuple(args, "h", &cam_num)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    char cam_name[CAM_NAME_LEN];
    if (!pl_cam_get_name(cam_num, cam_name)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    return PyUnicode_FromString(cam_name);
}

/**
  This function will open a camera given its name. A camera handle will be
  returned upon opening.
  @return Handle of camera.
*/
static PyObject *
pvc_open_camera(PyObject *self, PyObject *args)
{
    char *cam_name;
    if (!PyArg_ParseTuple(args, "s", &cam_name)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    int16 hCam;
    /* Note that OPEN_EXCLUSIVE is the only available open mode in PVCAM. */
    if (!pl_cam_open(cam_name, &hCam, OPEN_EXCLUSIVE)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }

    // Add defaulted Cam_Instance_T
    g_camInstanceMap[hCam];

    return PyLong_FromLong(hCam);
}

/**
  This function will close a camera given its handle.
*/
static PyObject *
pvc_close_camera(PyObject *self, PyObject *args)
{
    int16 hCam;
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    // Clear instance data
    g_camInstanceMap.erase(hCam);

    if (!pl_cam_close(hCam)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
  This function will get a specified parameter and return its value.
  @return The value of the specified parameter.
*/
static PyObject *
pvc_get_param(PyObject *self, PyObject *args)
{
    int16 hCam;
    uns32 param_id;
    int16 param_attribute;
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "hih", &hCam, &param_id, &param_attribute)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    /* Check if the camera supports the setting. Raise an AttributeError if
       it does not. Make sure camera is open first; otherwise AttributeError
       will be raised for not having an open camera. Let that error fall
       through to the ATTR_TYPE call, where the error message will be set and
       the appropriate error will be raised.*/
    if (!is_avail(hCam, param_id)) {
        PyErr_SetString(PyExc_AttributeError,
            "Invalid setting for this camera.");
        return NULL;
    }
    /* If the data type returned is a string, return a PyUnicode object.
       Otherwise, assume it is a number of some sort. */
    uns16 ret_type;
    if (!pl_get_param(hCam, param_id, ATTR_TYPE, (void *)&ret_type)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }

    Param_Val_T param_val;
    if (!pl_get_param(hCam, param_id, param_attribute, (void *)&param_val)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }

    switch(ret_type){
      case TYPE_CHAR_PTR:
        return PyUnicode_FromString(param_val.val_str);

      case TYPE_ENUM:
        return PyLong_FromLong(param_val.val_enum);

      case TYPE_INT8:
        return PyLong_FromLong(param_val.val_int8);

      case TYPE_UNS8:
        return PyLong_FromUnsignedLong(param_val.val_uns8);

      case TYPE_INT16:
        return PyLong_FromLong(param_val.val_int16);

      case TYPE_UNS16:
        return PyLong_FromUnsignedLong(param_val.val_uns16);

      case TYPE_INT32:
        return PyLong_FromLong(param_val.val_int32);

      case TYPE_UNS32:
        return PyLong_FromUnsignedLong(param_val.val_uns32);

      case TYPE_INT64:
        return PyLong_FromLongLong(param_val.val_long64);

      case TYPE_UNS64:
        return PyLong_FromUnsignedLongLong(param_val.val_ulong64);

      case TYPE_FLT32:
          return PyLong_FromDouble(param_val.val_flt32);

      case TYPE_FLT64:
        return PyLong_FromDouble(param_val.val_flt64);

      case TYPE_BOOLEAN:
        if (param_val.val_bool)
          Py_RETURN_TRUE;
        else
          Py_RETURN_FALSE;
    }

    PyErr_SetString(PyExc_RuntimeError, "Failed to match datatype");
    return NULL;
}

/**
  This function will set a specified parameter to a given value.
*/
static PyObject *
pvc_set_param(PyObject *self, PyObject *args)
{
    int16 hCam;
    uns32 param_id;
    void *param_value;
    /* Build the string to determine the type of the parameter value. */
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "hii", &hCam, &param_id, &param_value)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    /* Check if the camera supports the setting. Raise an AttributeError if
    it does not. Make sure camera is open first; otherwise AttributeError
    will be raised for not having an open camera. Let that error fall
    through to the pl_set_param call, where the error message will be set and
    the appropriate error will be raised.*/
    if (!is_avail(hCam, param_id)) {
        PyErr_SetString(PyExc_AttributeError,
            "Invalid setting for this camera.");
        return NULL;
    }
    if (!pl_set_param(hCam, param_id, &param_value)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
  This function will check if a specified parameter is available.
*/
static PyObject *
pvc_check_param(PyObject *self, PyObject *args)
{
    int16 hCam;
    uns32 param_id;

    /* Build the string to determine the type of the parameter value. */
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "hi", &hCam, &param_id)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    /* Check if the camera supports the setting. Raise an AttributeError if
    it does not. Make sure camera is open first; otherwise AttributeError
    will be raised for not having an open camera. Let that error fall
    through to the pl_set_param call, where the error message will be set and
    the appropriate error will be raised.*/
    rs_bool avail;
    /* Do not return falsy if a failed call to pl_get_param is made.
       Only return falsy if avail is falsy. */
    if (!pl_get_param(hCam, param_id, ATTR_AVAIL, (void *)&avail))
        Py_RETURN_TRUE;

    if (avail)
      Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *
pvc_start_live(PyObject *self, PyObject *args)
{
    int16 hCam;                 /* Camera handle. */
    PyObject * roiListObj;      /* the list of ROIs */
    uns32 expTime;              /* Exposure time. */
    int16 expMode;              /* Exposure mode. */
    uns16 buffer_frame_count;   /* Number of frames in the circular frame buffer. */
    char* stream_to_disk_path;  /* Path and filename to store frames */

    if (!PyArg_ParseTuple(args, "hO!IhHz", &hCam, &PyList_Type, &roiListObj, &expTime, &expMode, &buffer_frame_count, &stream_to_disk_path)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    if (!pl_cam_register_callback_ex3(hCam, PL_CALLBACK_EOF, (void *)NewFrameHandler, NULL))
    {
        PyErr_SetString(PyExc_ValueError, "Could not register call back.");
        return NULL;
    }

    /* Construct ROI list */
    uns16 rgn_total = (uns16)PyList_Size(roiListObj);
    std::unique_ptr<rgn_type[]> roiArray(new rgn_type[rgn_total]);
    populateRegions(roiArray.get(), rgn_total, roiListObj);

    /* Setup the acquisition. */
    uns32 exposureBytes;
    if (!pl_exp_setup_cont(hCam, rgn_total, roiArray.get(), expMode, expTime, &exposureBytes, CIRC_OVERWRITE)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }

    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(hCam);

        if (!check_meta_data_enabled(hCam, camInstance.metaDataEnabled_)) {
            PyErr_SetString(PyExc_MemoryError, "Unable to query meta data enabled.");
            return NULL;
        }

        uns32 buffer_size = buffer_frame_count * exposureBytes;
        if (!camInstance.allocateFrameBuffer(buffer_size, buffer_frame_count, exposureBytes))
        {
            PyErr_SetString(PyExc_MemoryError, "Unable to allocate memory for frame.");
            return NULL;
        }

        if (!camInstance.setStreamToDisk(stream_to_disk_path))
        {
            PyErr_SetString(PyExc_MemoryError, "Unable to set stream to disk. Check file path.");
            return NULL;
        }

        camInstance.prevTime_ = std::chrono::high_resolution_clock::now();

        if (!pl_exp_start_cont(hCam, camInstance.frameBuffer_, buffer_size)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }

        camInstance.setAcquisitiontMode(false);
    }
    catch (const std::out_of_range& oor) {
        PyErr_SetString(PyExc_KeyError, oor.what());
        return NULL;
    }

    return PyLong_FromLong(exposureBytes);
}

/**
 * Starts collection of a frames in sequence.
 */
static PyObject *
pvc_start_seq(PyObject *self, PyObject *args)
{
    int16 hCam;            /* Camera handle. */
    PyObject * roiListObj; /* the list of ROIs */
    uns32 expTime;         /* Exposure time. */
    int16 expMode;         /* Exposure mode. */
    uns16 expTotal;        /* Total frames */
    if (!PyArg_ParseTuple(args, "hO!IhH", &hCam, &PyList_Type, &roiListObj, &expTime, &expMode, &expTotal)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    if (!pl_cam_register_callback_ex3(hCam, PL_CALLBACK_EOF, (void *)NewFrameHandler, NULL))
    {
        PyErr_SetString(PyExc_ValueError, "Could not register call back.");
        return NULL;
    }

    /* Construct ROI list */
    uns16 rgn_total = (uns16)PyList_Size(roiListObj);
    std::unique_ptr<rgn_type[]> roiArray(new rgn_type[rgn_total]);
    populateRegions(roiArray.get(), rgn_total, roiListObj);

    /* Setup the acquisition. */
    uns32 exposureBytes;
    if (!pl_exp_setup_seq(hCam, expTotal, rgn_total, roiArray.get(), expMode, expTime, &exposureBytes)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    uns32 exposureBytesPerFrame = exposureBytes / expTotal;

    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(hCam);

        if (!check_meta_data_enabled(hCam, camInstance.metaDataEnabled_)) {
            PyErr_SetString(PyExc_MemoryError, "Unable to query meta data enabled.");
            return NULL;
        }

        if (!camInstance.allocateFrameBuffer(exposureBytes, expTotal, exposureBytesPerFrame))
        {
            PyErr_SetString(PyExc_MemoryError, "Unable to allocate memory for frame.");
            return NULL;
        }

        camInstance.prevTime_ = std::chrono::high_resolution_clock::now();

        if (!pl_exp_start_seq(hCam, camInstance.frameBuffer_)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }

        camInstance.setAcquisitiontMode(true);
    }
    catch (const std::out_of_range& oor) {
        PyErr_SetString(PyExc_KeyError, oor.what());
        return NULL;
    }

    return PyLong_FromLong(exposureBytesPerFrame);
}

static PyObject *
pvc_check_frame_status(PyObject *self, PyObject *args)
{
    char* statusStr;

    int16 hCam;                /* Camera handle. */
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
            PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
            return NULL;
    }

    rs_bool checkStatusResult = PV_OK;
    int16 status = READOUT_NOT_ACTIVE;
    uns32 bytes_arrived;

    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(hCam);

        if (camInstance.seqMode_) {
            checkStatusResult = pl_exp_check_status(hCam, &status, &bytes_arrived);
        }
        else {
            uns32 buffer_cnt;
            checkStatusResult = pl_exp_check_cont_status(hCam, &status, &bytes_arrived, &buffer_cnt);
        }

        if (checkStatusResult == PV_OK) {
            switch (status) {
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
                if (camInstance.seqMode_) {
                    statusStr = "READOUT_COMPLETE";
                } else {
                    statusStr = "FRAME_AVAILABLE";
                }
                break;
            default:
                PyErr_SetString(PyExc_ValueError, "Unrecognized frame status.");
                return NULL;
            }
        }
        else {
            switch (status) {
            case READOUT_FAILED:
                statusStr = "READOUT_FAILED";
                break;
            default:
                set_g_msg();
                PyErr_SetString(PyExc_RuntimeError, g_msg);
                return NULL;
            }
        }
    }
    catch (const std::out_of_range& oor) {
        PyErr_SetString(PyExc_KeyError, oor.what());
        return NULL;
    }

    return PyUnicode_FromString(statusStr);
}

static PyObject *
pvc_get_frame(PyObject *self, PyObject *args)
{
    int16 hCam;                /* Camera handle. */
    PyObject * roiListObj;     /* The list of ROIs */
    int typenum;               /* Numpy typenum specifying data type for image data */
    int timeout_ms;            /* Poll frame timeout in ms. Negative values will wait forever. */
    bool oldestFrame;

    if (!PyArg_ParseTuple(args, "hO!iip", &hCam, &PyList_Type, &roiListObj, &typenum, &timeout_ms, &oldestFrame)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    import_array();  /* Initialize PyArrayObject. */

    // WARNING. We are accessing a camera instance below without locking the object so that
    //          the new frame handler or abort can take the lock and alter date. Care must be
    //          taken to not alter the camera instance until the lock is obtained
    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(hCam);

        // Poll camera readout status
        // RL Add timeout to polling loop based on expected frame return time

        /* Release the GIL to allow other Python threads to run */
        /* This macro has an open brace and must be paired */
        int16 status;
        uns32 byte_cnt;
        rs_bool checkStatusResult = pl_exp_check_status(hCam, &status, &byte_cnt);

        // Wait for frame in a loop if using a timeout and new data isn't available and abort hasn't occurred
        if (timeout_ms != 0 && !camInstance.newData_ && !camInstance.abortData_)
        {
            int32 elapsed_ms = 0;
            Py_BEGIN_ALLOW_THREADS
            while (checkStatusResult == PV_OK) {
                // We want to wait for a new frame, but every so often check if readout failed
                std::unique_lock<std::mutex> lock(camInstance.frameMutex_);
                static const int READOUT_FAILED_TIMEOUT_ms = 5000;
                camInstance.conditionalVariable_.wait_for(lock, std::chrono::milliseconds(READOUT_FAILED_TIMEOUT_ms));
                elapsed_ms += READOUT_FAILED_TIMEOUT_ms;

                // Exit loop if read out is finished, new data available, abort occurred or timeout expires,
                checkStatusResult = pl_exp_check_status(hCam, &status, &byte_cnt);
                if (status == READOUT_FAILED || status == READOUT_COMPLETE || camInstance.newData_ || camInstance.abortData_ ||
                   (elapsed_ms >= timeout_ms && timeout_ms > 0)) {
                    break;
                }
            }
            Py_END_ALLOW_THREADS
        }
        std::lock_guard<std::mutex> lock(g_camInstanceMutex);

        if (checkStatusResult == PV_FAIL) {
            camInstance.newData_ = false;
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        else if (status == READOUT_FAILED) {
            camInstance.newData_ = false;
            PyErr_SetString(PyExc_RuntimeError, "get_frame() readout failed.");
            return NULL;
        }
        else if (camInstance.abortData_) {
            camInstance.abortData_ = false;
            camInstance.newData_ = false;
            PyErr_SetString(PyExc_RuntimeError, "frame aborted.");
            return NULL;
        }
        else if (status == READOUT_COMPLETE && !camInstance.newData_) {
            camInstance.abortData_ = false;
            PyErr_SetString(PyExc_RuntimeError, "frame callback not called. Frame was likely aborted in PVCAM due to host command. Check log.");
            return NULL;
        }
        else if (!camInstance.newData_) {
            camInstance.abortData_ = false;
            PyErr_SetString(PyExc_RuntimeError, "frame timeout. Verify timeout exceeds exposure time. If applicable, check external trigger source.");
            return NULL;
        }

        Frame_T frame;
        if (oldestFrame) {
            frame = camInstance.frameQueue_.front();
            camInstance.frameQueue_.pop();
        } else {
            frame = camInstance.frameQueue_.back();
        }

        //printf("New Data FPS: %f Cnt: %d\r\n", camInstance.fps_, frame.count);

        // Toggle newData_ flag unless we are in sequence mode and another frame is available
        camInstance.newData_ = camInstance.seqMode_ && !camInstance.frameQueue_.empty();
        
        PyObject *frameDict = PyDict_New();
        PyObject* roiDataList = PyList_New(0);
        const int NUM_DIMS = 2;
        if (camInstance.metaDataEnabled_) {

            PyObject* frame_header = PyDict_New();
            PyObject* roiHeaderList = PyList_New(0);
            PyObject* meta_data = PyDict_New();
            PyDict_SetItem(meta_data, PyUnicode_FromString("frame_header"), frame_header);
            PyDict_SetItem(meta_data, PyUnicode_FromString("roi_headers"), roiHeaderList);
            PyDict_SetItem(frameDict, PyUnicode_FromString("meta_data"), meta_data);

            camInstance.initializeMetaData();
            if (!pl_md_frame_decode(&camInstance.mdFrame_, frame.address, camInstance.frameSize_)) {
                PyErr_SetString(PyExc_RuntimeError, "Meta data decode failed.");
                return NULL;
            }

            populateMetaDataFrameHeader(camInstance.mdFrame_.header, frame_header);

            for (uns32 i = 0; i < camInstance.mdFrame_.header->roiCount; i++) {

                md_frame_roi_header* pMetaDataRoiHeader = camInstance.mdFrame_.roiArray[i].header;
                PyObject* roi_header = PyDict_New();
                populateMetaDataRoiHeader(pMetaDataRoiHeader, roi_header);
                PyList_Append(roiHeaderList, roi_header);

                npy_intp pixelsS = (pMetaDataRoiHeader->roi.s2 - pMetaDataRoiHeader->roi.s1 + 1) / pMetaDataRoiHeader->roi.sbin;
                npy_intp pixelsP = (pMetaDataRoiHeader->roi.p2 - pMetaDataRoiHeader->roi.p1 + 1) / pMetaDataRoiHeader->roi.pbin;
                npy_intp dims[NUM_DIMS] = {pixelsP, pixelsS};
                PyObject* numpy_frame = (PyObject *)PyArray_SimpleNewFromData(NUM_DIMS, dims, typenum, camInstance.mdFrame_.roiArray[i].data);
                PyList_Append(roiDataList, numpy_frame);
            }
        }
        else
        {
            /* Construct ROI list. Since meta data is disabled, there can only be 1 region */
            rgn_type roi;
            populateRegions(&roi, 1, roiListObj);

            npy_intp pixelsS = (roi.s2 - roi.s1 + 1) / roi.sbin;
            npy_intp pixelsP = (roi.p2 - roi.p1 + 1) / roi.pbin;
            npy_intp dims[NUM_DIMS] = {pixelsP, pixelsS};
            PyObject* numpy_frame = (PyObject *)PyArray_SimpleNewFromData(NUM_DIMS, dims, typenum, frame.address);
            PyList_Append(roiDataList, numpy_frame);
        }

        PyDict_SetItem(frameDict, PyUnicode_FromString("pixel_data"), roiDataList);

        PyObject *fps = PyFloat_FromDouble(camInstance.fps_);
        PyObject *frame_count = PyLong_FromLong(frame.count);

        PyObject *tup = PyTuple_New(3);
        PyTuple_SetItem(tup, 0, frameDict);
        PyTuple_SetItem(tup, 1, fps);
        PyTuple_SetItem(tup, 2, frame_count);

        return tup;
    }
    catch (const std::out_of_range& oor) {
        PyErr_SetString(PyExc_KeyError, oor.what());
        return NULL;
    }
}

static PyObject *
pvc_stop_live(PyObject *self, PyObject *args)
{
    int16 hCam;    /* Camera handle. */
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    if (PV_OK != pl_exp_stop_cont(hCam, CCS_CLEAR)) {    //stop the circular buffer aquisition
        PyErr_SetString(PyExc_ValueError, "Buffer failed to stop");
        return NULL;
    }

    if (!pl_cam_deregister_callback(hCam, PL_CALLBACK_EOF))
    {
        PyErr_SetString(PyExc_ValueError, "Could not deregister call back.");
        return NULL;
    }

    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(hCam);
        camInstance.cleanUpFrameBuffer();
        camInstance.unsetStreamToDisk();
    }
    catch (const std::out_of_range& oor) {
        PyErr_SetString(PyExc_KeyError, oor.what());
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
pvc_finish_seq(PyObject *self, PyObject *args)
{
    int16 hCam;    /* Camera handle. */
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(hCam);

        // Abort acquisition if necessary
        int16 status;
        uns32 byte_cnt;
        rs_bool checkStatusResult = pl_exp_check_status(hCam, &status, &byte_cnt);
        if (checkStatusResult == PV_OK) {
            if (status == EXPOSURE_IN_PROGRESS || status == READOUT_IN_PROGRESS) {

                if (PV_OK != pl_exp_abort(hCam, CCS_HALT)) {   //stop the circular buffer aquisition
                    PyErr_SetString(PyExc_ValueError, "Failed to abort");
                    return NULL;
                }
            }
        }

        if (!pl_cam_deregister_callback(hCam, PL_CALLBACK_EOF))
        {
            PyErr_SetString(PyExc_ValueError, "Could not deregister call back.");
            return NULL;
        }

        if (PV_OK != pl_exp_finish_seq(hCam, camInstance.frameBuffer_, NULL)) {
            PyErr_SetString(PyExc_ValueError, "Failed to finish sequence");
            return NULL;
        }

        camInstance.cleanUpFrameBuffer();
    }
    catch (const std::out_of_range& oor) {
        PyErr_SetString(PyExc_KeyError, oor.what());
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
pvc_abort(PyObject *self, PyObject *args)
{
    int16 hCam;    /* Camera handle. */
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }

    if (PV_OK != pl_exp_abort(hCam, CCS_HALT)) {   //stop the circular buffer aquisition
        PyErr_SetString(PyExc_ValueError, "Failed to abort");
        return NULL;
    }

    if (!pl_cam_deregister_callback(hCam, PL_CALLBACK_EOF))
    {
        PyErr_SetString(PyExc_ValueError, "Could not deregister call back.");
        return NULL;
    }

    std::lock_guard<std::mutex> lock(g_camInstanceMutex);
    try {
        Cam_Instance_T& camInstance = g_camInstanceMap.at(hCam);
        camInstance.cleanUpFrameBuffer();
        camInstance.unsetStreamToDisk();

        camInstance.abortData_ = true;
        camInstance.conditionalVariable_.notify_all();
    }
    catch (const std::out_of_range& oor) {
        PyErr_SetString(PyExc_KeyError, oor.what());
        return NULL;
    }
    Py_RETURN_NONE;
}

/** set_exp_out_mode
 *
 * Used to set the exposure out mode of a camera.
 *
 * Since exp_out_mode is a read only parameter, the only way to change it is
 * by setting up an acquisition and providing the desired exposure out mode
 * there.
 */
static PyObject *
pvc_set_exp_modes(PyObject *self, PyObject *args)
{
    /* The arguments supplied to this function from python function call are:
     *   hcam: The handle of the camera to change the expose out mode of.
     *   mode: The bit-wise or between exposure mode and expose out mode
     */

     int16 hcam;    /* Camera handle. */
     int16 expMode; /* Exposure mode. */
     if (!PyArg_ParseTuple(args, "hh", &hcam, &expMode)) {
         PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
         return NULL;
     }
     /* Struct that contains the frame size and binning information. */
     rgn_type frame = {0, 0, 1, 0, 0, 1};
     uns32 exposureBytes;

     /* Setup the acquisition. */
     if (!pl_exp_setup_seq(hcam, 1, 1, &frame, expMode, 0, &exposureBytes)) {
         set_g_msg();
         PyErr_SetString(PyExc_RuntimeError, g_msg);
         return NULL;
     }
     pl_exp_abort(hcam, CCS_HALT);

     Py_RETURN_NONE;
}


/** valid_enum_param
 *
 * Helper function that determines if a given value is a valid selection for an
 * enumerated type. Should any PVCAM function calls in this function fail, a
 * falsy value will be returned.
 *
 * Parameters:
 *   hCam: The handle of the camera in question.
 *   param_id: The enumerated parameter to check.
 *   selected_val: The value to check if it is a valid selection.
 *
 *  Returns:
 *   0 if selection is not a valid instance of the enumerated type.
 *   1 if selection is a valid instance of the enumerated type.
 */
int valid_enum_param(int16 hCam, uns32 param_id, int32 selected_val)
{
    /* If the enum param is not available return False. */
    rs_bool param_avail = FALSE;
    if (!pl_get_param(hCam, param_id, ATTR_AVAIL, &param_avail)
            || param_avail == FALSE) {
        return 0;
    }
    /* Get the number of valid modes for the setting. */
    uns32 num_selections = 0;
    if (!pl_get_param(hCam, param_id, ATTR_COUNT, &num_selections)) {
        return 0;
    }
    /* Loop over all of the possible selections and see if any match with the
     * selection provided. */
    for (uns32 i=0; i < num_selections; i++) {
        /* Enum name string is required for pl_get_enum_param function. */
        uns32 enum_str_len;
        if (!pl_enum_str_length(hCam, param_id, i, &enum_str_len)) {
            return 0;
        }
        char * enum_str = new char[enum_str_len];
        int32 enum_val;
        if (!pl_get_enum_param(hCam, param_id, i, &enum_val,
                               enum_str, enum_str_len)) {
            return 0;
        }
        /* If the selected value parameter matches any of the valid enum values,
         * then return 1. */
        if (selected_val == enum_val) {
            return 1;
        }
    }
    return 0; /* If a match was never found, return 0. */
}

static PyObject *
pvc_read_enum(PyObject *self, PyObject *args)
{
    int16 hCam;
    uns32 param_id;
    if (!PyArg_ParseTuple(args, "hi", &hCam, &param_id)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    if (!is_avail(hCam, param_id)){
        PyErr_SetString(PyExc_AttributeError, "Invalid setting for camera.");
        return NULL;
    }
    uns32 count;
    if (!pl_get_param(hCam, param_id, ATTR_COUNT, (void *)&count)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }

    PyObject *result = PyDict_New();
    for (uns32 i = 0; i < count; i++) {
        // Get the length of the name of the parameter.
        uns32 str_len;
        if (!pl_enum_str_length(hCam, param_id, i, &str_len)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }

        // Allocate the destination string
        char *name = new (std::nothrow) char[str_len];

        // Get string and value
        int32 value;
        if (!pl_get_enum_param(hCam, param_id, i, &value, name, str_len)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        PyObject *pyName = PyUnicode_FromString(name);
        PyObject *pyValue = PyLong_FromSize_t(value);

        PyDict_SetItem(result, pyName, pyValue);
    }
    return result;
}

/**
  This function will reset all prost-processing features of the open camera.
*/
static PyObject *
pvc_reset_pp(PyObject *self, PyObject *args)
{
    int16 hCam;
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    if (!pl_pp_reset(hCam)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
pvc_sw_trigger(PyObject *self, PyObject *args)
{
    int16 hCam;
    if (!PyArg_ParseTuple(args, "h", &hCam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    uns32 flags = 0;
    uns32 value = 0;
    rs_bool result = pl_exp_trigger(hCam, &flags, value);

    if (result != PV_OK) {
        PyErr_SetString(PyExc_ValueError, "Failed to deliver software trigger.");
        return NULL;
    }
    else if (flags != PL_SW_TRIG_STATUS_TRIGGERED) {
        PyErr_SetString(PyExc_ValueError, "Failed to perform software trigger.");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
This function will return true upon success.
*/
bool check_meta_data_enabled(int16 hCam, bool& metaDataEnabled)
{
    metaDataEnabled = false;

    if (is_avail(hCam, PARAM_METADATA_ENABLED)) {

        Param_Val_T param_val;
        if (!pl_get_param(hCam, PARAM_METADATA_ENABLED, ATTR_CURRENT, (void *)&param_val)) {
            return false;
        }
        metaDataEnabled = param_val.val_bool != FALSE;
        return true;
    }

    return true;
}

void populateMetaDataFrameHeader(md_frame_header* pMetaDataFrameHeader, PyObject* frame_header)
{
    PyDict_SetItem(frame_header, PyUnicode_FromString("signature"), PyUnicode_FromString(reinterpret_cast<const char*>(&pMetaDataFrameHeader->signature)));
    PyDict_SetItem(frame_header, PyUnicode_FromString("version"), PyLong_FromLong(pMetaDataFrameHeader->version));
    PyDict_SetItem(frame_header, PyUnicode_FromString("frameNr"), PyLong_FromLong(pMetaDataFrameHeader->frameNr));
    PyDict_SetItem(frame_header, PyUnicode_FromString("roiCount"), PyLong_FromLong(pMetaDataFrameHeader->roiCount));

    if (pMetaDataFrameHeader->version < 3) {
        PyDict_SetItem(frame_header, PyUnicode_FromString("timestampBOF"), PyLong_FromLong(pMetaDataFrameHeader->timestampBOF));
        PyDict_SetItem(frame_header, PyUnicode_FromString("timestampEOF"), PyLong_FromLong(pMetaDataFrameHeader->timestampEOF));
        PyDict_SetItem(frame_header, PyUnicode_FromString("timestampResNs"), PyLong_FromLong(pMetaDataFrameHeader->timestampResNs));
        PyDict_SetItem(frame_header, PyUnicode_FromString("exposureTime"), PyLong_FromLong(pMetaDataFrameHeader->exposureTime));
        PyDict_SetItem(frame_header, PyUnicode_FromString("exposureTimeResNs"), PyLong_FromLong(pMetaDataFrameHeader->exposureTimeResNs));
        PyDict_SetItem(frame_header, PyUnicode_FromString("roiTimestampResNs"), PyLong_FromLong(pMetaDataFrameHeader->roiTimestampResNs));
        PyDict_SetItem(frame_header, PyUnicode_FromString("bitDepth"), PyLong_FromLong(pMetaDataFrameHeader->bitDepth));
        PyDict_SetItem(frame_header, PyUnicode_FromString("colorMask"), PyLong_FromLong(pMetaDataFrameHeader->colorMask));
        PyDict_SetItem(frame_header, PyUnicode_FromString("flags"), PyLong_FromLong(pMetaDataFrameHeader->flags));
        PyDict_SetItem(frame_header, PyUnicode_FromString("extendedMdSize"), PyLong_FromLong(pMetaDataFrameHeader->extendedMdSize));

        if (pMetaDataFrameHeader->version > 1) {
            PyDict_SetItem(frame_header, PyUnicode_FromString("imageFormat"), PyLong_FromLong(pMetaDataFrameHeader->imageFormat));
            PyDict_SetItem(frame_header, PyUnicode_FromString("imageCompression"), PyLong_FromLong(pMetaDataFrameHeader->imageCompression));
        }
    } else {
        md_frame_header_v3* pMetaDataFrameHeaderV3 = reinterpret_cast<md_frame_header_v3*>(pMetaDataFrameHeader);
        PyDict_SetItem(frame_header, PyUnicode_FromString("timestampBOF"), PyLong_FromUnsignedLongLong(pMetaDataFrameHeaderV3->timestampBOF));
        PyDict_SetItem(frame_header, PyUnicode_FromString("timestampEOF"), PyLong_FromUnsignedLongLong(pMetaDataFrameHeaderV3->timestampEOF));
        PyDict_SetItem(frame_header, PyUnicode_FromString("exposureTime"), PyLong_FromUnsignedLongLong(pMetaDataFrameHeaderV3->exposureTime));
        PyDict_SetItem(frame_header, PyUnicode_FromString("bitDepth"), PyLong_FromLong(pMetaDataFrameHeaderV3->bitDepth));
        PyDict_SetItem(frame_header, PyUnicode_FromString("colorMask"), PyLong_FromLong(pMetaDataFrameHeaderV3->colorMask));
        PyDict_SetItem(frame_header, PyUnicode_FromString("flags"), PyLong_FromLong(pMetaDataFrameHeaderV3->flags));
        PyDict_SetItem(frame_header, PyUnicode_FromString("extendedMdSize"), PyLong_FromLong(pMetaDataFrameHeaderV3->extendedMdSize));
        PyDict_SetItem(frame_header, PyUnicode_FromString("imageFormat"), PyLong_FromLong(pMetaDataFrameHeaderV3->imageFormat));
        PyDict_SetItem(frame_header, PyUnicode_FromString("imageCompression"), PyLong_FromLong(pMetaDataFrameHeaderV3->imageCompression));
    }
}

void populateMetaDataRoiHeader(md_frame_roi_header* pMetaDataRoiHeader, PyObject* roi_header)
{
    PyDict_SetItem(roi_header, PyUnicode_FromString("roiNr"), PyLong_FromLong(pMetaDataRoiHeader->roiNr));
    PyDict_SetItem(roi_header, PyUnicode_FromString("timestampBOR"), PyLong_FromLong(pMetaDataRoiHeader->timestampBOR));
    PyDict_SetItem(roi_header, PyUnicode_FromString("timestampEOR"), PyLong_FromLong(pMetaDataRoiHeader->timestampEOR));

    PyObject* roi = PyDict_New();
    PyDict_SetItem(roi, PyUnicode_FromString("s1"), PyLong_FromLong(pMetaDataRoiHeader->roi.s1));
    PyDict_SetItem(roi, PyUnicode_FromString("s2"), PyLong_FromLong(pMetaDataRoiHeader->roi.s2));
    PyDict_SetItem(roi, PyUnicode_FromString("sbin"), PyLong_FromLong(pMetaDataRoiHeader->roi.sbin));
    PyDict_SetItem(roi, PyUnicode_FromString("p1"), PyLong_FromLong(pMetaDataRoiHeader->roi.p1));
    PyDict_SetItem(roi, PyUnicode_FromString("p2"), PyLong_FromLong(pMetaDataRoiHeader->roi.p2));
    PyDict_SetItem(roi, PyUnicode_FromString("pbin"), PyLong_FromLong(pMetaDataRoiHeader->roi.pbin));
    PyDict_SetItem(roi_header, PyUnicode_FromString("roi"), roi);

    PyDict_SetItem(roi_header, PyUnicode_FromString("flags"), PyLong_FromLong(pMetaDataRoiHeader->flags));
    PyDict_SetItem(roi_header, PyUnicode_FromString("extendedMdSize"), PyLong_FromLong(pMetaDataRoiHeader->extendedMdSize));
    PyDict_SetItem(roi_header, PyUnicode_FromString("roiDataSize"), PyLong_FromLong(pMetaDataRoiHeader->roiDataSize));
}

void populateRegions(rgn_type* roiArray, uns16 numRois, PyObject * roiListObj)
{
    for (uns32 i = 0; i < numRois; i++)
    {
        PyObject* roiObj = PyList_GetItem(roiListObj, i);
        PyObject* s1Obj = PyObject_GetAttrString(roiObj, "s1");
        PyObject* s2Obj = PyObject_GetAttrString(roiObj, "s2");
        PyObject* sbinObj = PyObject_GetAttrString(roiObj, "sbin");
        PyObject* p1Obj = PyObject_GetAttrString(roiObj, "p1");
        PyObject* p2Obj = PyObject_GetAttrString(roiObj, "p2");
        PyObject* pbinObj = PyObject_GetAttrString(roiObj, "pbin");

        roiArray[i].s1 = (uns16)PyLong_AsLong(s1Obj);
        roiArray[i].s2 = (uns16)PyLong_AsLong(s2Obj);
        roiArray[i].sbin = (uns16)PyLong_AsLong(sbinObj);
        roiArray[i].p1 = (uns16)PyLong_AsLong(p1Obj);
        roiArray[i].p2 = (uns16)PyLong_AsLong(p2Obj);
        roiArray[i].pbin = (uns16)PyLong_AsLong(pbinObj);
    }
}

/* When writing a new function, include it in the Method Table definitions!
 *
 * The method table is partially responsible for allowing Python programs to
 * call functions from an extension module. It does by creating PyMethodDef
 * structs with four fields:
 * 1. ml_name -- char * -- name of the method
 * 2. ml_meth -- PyCFunction -- pointer to the C implementation
 * 3. ml_flags -- int -- flag bits indicating how the call should be constructed
 * 4. ml_doc -- char * -- points to the contents of the docstring
 *
 * The ml_name is the name of the function by which a Python program can call
 * the function at the address of ml_meth.
 *
 * The ml_meth is a C function pointer that will always return a PyObject*.
 *
 * The ml_flags field is a bitfield that indicate a calling convention.
 * Generally, METH_VARARGS or METH_NOARGS will be used.
 *
 * The list of PyMethodDef's then passed into the PyModuleDef, which defines
 * all of the information needed to create a module object.
 */
 // Function that gets called from PVCAM when EOF event arrives

static PyMethodDef PvcMethods[] = {
    {"get_pvcam_version",
        pvc_get_pvcam_version,
        METH_NOARGS,
        get_pvcam_version_docstring},
    {"get_cam_fw_version",
        pvc_get_cam_fw_version,
        METH_VARARGS,
        get_cam_fw_version_docstring},
    {"get_cam_total",
        pvc_get_cam_total,
        METH_NOARGS,
        get_cam_total_docstring},
    {"init_pvcam",
        pvc_init_pvcam,
        METH_NOARGS,
        init_pvcam_docstring},
    {"uninit_pvcam",
        pvc_uninit_pvcam,
        METH_NOARGS,
        uninit_pvcam_docstring},
    {"get_cam_name",
        pvc_get_cam_name,
        METH_VARARGS,
        get_cam_name_docstring},
    {"open_camera",
        pvc_open_camera,
        METH_VARARGS,
        open_camera_docstring},
    {"close_camera",
        pvc_close_camera,
        METH_VARARGS,
        close_camera_docstring},
    {"get_param",
        pvc_get_param,
        METH_VARARGS,
        get_param_docstring},
    {"set_param",
        pvc_set_param,
        METH_VARARGS,
        set_param_docstring},
    {"check_param",
        pvc_check_param,
        METH_VARARGS,
        check_param_docstring},
    {"check_frame_status",
        pvc_check_frame_status,
        METH_VARARGS,
        check_frame_status_docstring},
    {"get_frame",
        pvc_get_frame,
        METH_VARARGS,
        get_frame_docstring},
    {"start_live",
        pvc_start_live,
        METH_VARARGS,
        start_live_docstring},
    {"start_seq",
        pvc_start_seq,
        METH_VARARGS,
        start_seq_docstring},
    {"stop_live",
        pvc_stop_live,
        METH_VARARGS,
        stop_live_docstring },
    {"finish_seq",
        pvc_finish_seq,
        METH_VARARGS,
        finish_seq_docstring },
    {"abort",
        pvc_abort,
        METH_VARARGS,
        abort_docstring },
    {"set_exp_modes",
        pvc_set_exp_modes,
        METH_VARARGS,
        set_exp_modes_docstring},
    {"read_enum",
        pvc_read_enum,
        METH_VARARGS,
        read_enum_docstring},
    {"reset_pp",
        pvc_reset_pp,
        METH_VARARGS,
        reset_pp_docstring},
    {"sw_trigger",
        pvc_sw_trigger,
        METH_VARARGS,
        sw_trigger_docstring},

    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pvcmodule = {
    PyModuleDef_HEAD_INIT,
    "pvc",            // Name of module
    module_docstring, // Module Documentation
    -1,               // Module keeps state in global variables
    PvcMethods        // Our list of module functions.
};

PyMODINIT_FUNC
PyInit_pvc(void)
{
    return PyModule_Create(&pvcmodule);
}
