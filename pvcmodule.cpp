// Local
#include "pvcmodule.h"
#include "backend/AcqHelper.h"
#include "backend/ConsoleLogger.h"
#include "backend/Log.h"
#include "backend/Frame.h"

// System
#include <new>
#include <iostream>
#include <chrono>
#include <thread>

/*
Global variables
*/
char g_msg[ERROR_MSG_LEN]; // Error Message Variable.
uns16 *g_frameAddress;	/*Address of the frame*/
uns16 *g_singleFrameAddress;	/*Address of the frame*/
auto g_prevTime = std::chrono::high_resolution_clock::now();
auto g_curTime = std::chrono::high_resolution_clock::now();
std::chrono::duration<double> g_timeDelta;
int g_frameCnt = 0;
double g_FPS = 0;
int g_live = 0; // flag [1 for active, 0 for inactive]

/** Sets the global error message. */
void set_g_msg(void) { pl_error_message(pl_error_code(), g_msg); }

typedef struct SampleContext
{
   int myData1;
   int myData2;
}
SampleContext;

void NewFrameHandler(FRAME_INFO *pFrameInfo, void *context)
{
    g_frameCnt++;
    g_curTime = std::chrono::high_resolution_clock::now();
    g_timeDelta = std::chrono::duration_cast<std::chrono::duration<double>>(g_curTime - g_prevTime);

    if (g_timeDelta.count() >= 0.25){
        g_FPS = g_frameCnt/g_timeDelta.count();
        //printf("framecnt: %d\nfps: %lf\ntime: %lf", g_frameCnt,g_FPS,g_timeDelta);
        g_frameCnt = 0;
        g_prevTime = g_curTime;
    }

    if (PV_OK != pl_exp_get_latest_frame(pFrameInfo->hCam, (void **)&g_frameAddress)) {
        PyErr_SetString(PyExc_ValueError, "Failed to get latest frame");
  	}
}

/** Returns true if the specified attribute is available. */
int is_avail(int16 hcam, uns32 param_id)
{
    rs_bool avail;
    /* Do not return falsy if a failed call to pl_get_param is made.
       Only return falsy if avail is falsy. */
    if (!pl_get_param(hcam, param_id, ATTR_AVAIL, (void *)&avail))
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
	int16 hcam;
	if (!PyArg_ParseTuple(args, "h", &hcam)) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
		return NULL;
	}
	uns16 ver_num;
	if (!pl_get_param(hcam, PARAM_CAM_FW_VERSION, ATTR_CURRENT, (void *)&ver_num)) {}

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
    char cam_name[CAM_NAME_LEN];
    /* Parse the arguments provided by the user, raising an error if
       PyArg_ParseTuple unsuccessfully parses the arguments. The second argument
       to the function call ("i") denotes the types of the incoming argument
       to the function; "i" = int. */
    if (!PyArg_ParseTuple(args, "h", &cam_num)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
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
    /* Parse the arguments provided by the user, raising an error if
       PyArg_ParseTuple unsuccessfully parses the arguments. The second argument
       to the function call ("s") denotes the types of the incoming argument
       to the function; "s" = string. */
    if (!PyArg_ParseTuple(args, "s", &cam_name)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    int16 hcam;
    /* Note that OPEN_EXCLUSIVE is the only available open mode in PVCAM. */
    if (!pl_cam_open(cam_name, &hcam, OPEN_EXCLUSIVE)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    return PyLong_FromLong(hcam);
}

/**
  This function will close a camera given its handle.
*/
static PyObject *
pvc_close_camera(PyObject *self, PyObject *args)
{
    int16 hcam;
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "h", &hcam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    if (!pl_cam_close(hcam)) {
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
    int16 hcam;
    uns32 param_id;
    int16 param_attribute;
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "hih", &hcam, &param_id, &param_attribute)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    /* Check if the camera supports the setting. Raise an AttributeError if
       it does not. Make sure camera is open first; otherwise AttributeError
       will be raised for not having an open camera. Let that error fall
       through to the ATTR_TYPE call, where the error message will be set and
       the appropriate error will be raised.*/
    if (!is_avail(hcam, param_id)) {
        PyErr_SetString(PyExc_AttributeError,
            "Invalid setting for this camera.");
        return NULL;
    }
    /* If the data type returned is a string, return a PyUnicode object.
       Otherwise, assume it is a number of some sort. */
    uns16 ret_type;
    if (!pl_get_param(hcam, param_id, ATTR_TYPE, (void *)&ret_type)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }

    // Have to change param_val's name because c99 support have dynamic initialize
    switch(ret_type){
      case TYPE_CHAR_PTR:
        char param_val_str[MAX_PP_NAME_LEN];
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_str)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_CHAR_PTR: %s\n\n", param_val_str);
        return PyUnicode_FromString(param_val_str);
        break;

      case TYPE_ENUM:
        uns16 param_val_enum;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_enum)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_ENUM: %d\n\n", param_val_enum);
        return PyLong_FromSize_t(param_val_enum);
        break;

      case TYPE_INT8:
        int8 param_val_int8;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_int8)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_INT8: %d\n\n", param_val_int8);
        return PyLong_FromSsize_t(param_val_int8);
        break;

      case TYPE_UNS8:
        uns8 param_val_uns8;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_uns8)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_UNS8: %d\n\n", param_val_uns8);
        return PyLong_FromSize_t(param_val_uns8);
        break;

      case TYPE_INT16:
        int16 param_val_int16;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_int16)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_INT16: %d\n\n", param_val_int16);
        return PyLong_FromSsize_t(param_val_int16);
        break;

      case TYPE_UNS16:
        uns16 param_val_uns16;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_uns16)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_UNS16: %d\n\n", param_val_uns16);
        return PyLong_FromSize_t(param_val_uns16);
        break;

      case TYPE_INT32:
        long param_val_int32;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_int32)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_INT32: %d\n\n", param_val_int32);
        return PyLong_FromLong(param_val_int32);
        break;

      case TYPE_UNS32:
        unsigned long param_val_uns32;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_uns32)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_UNS32: %d\n\n", param_val_uns32);
        return PyLong_FromUnsignedLong(param_val_uns32);
        break;

      case TYPE_INT64:
        long long param_val_int64;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_int64)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_INT64: %d\n\n", param_val_int64);
        return PyLong_FromLongLong(param_val_int64);
        break;

      case TYPE_UNS64:
        unsigned long long param_val_uns64;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_uns64)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_UNS64: %d\n\n", param_val_uns64);
        return PyLong_FromUnsignedLongLong(param_val_uns64);
        break;

      case TYPE_FLT64:
        double param_val_flt64;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_flt64)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_FLT64: %lf\n\n", param_val_flt64);
        return PyLong_FromDouble(param_val_flt64);
        break;

      case TYPE_BOOLEAN:
        rs_bool param_val_bool;
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val_bool)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        //printf("TYPE_BOOLEAN: %d\n\n", param_val_bool);

        if (param_val_bool)
          Py_RETURN_TRUE;
        else
          Py_RETURN_FALSE;
        break;
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
    int16 hcam;
    uns32 param_id;
    void *param_value;
    /* Build the string to determine the type of the parameter value. */
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "hii", &hcam, &param_id, &param_value)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    /* Check if the camera supports the setting. Raise an AttributeError if
    it does not. Make sure camera is open first; otherwise AttributeError
    will be raised for not having an open camera. Let that error fall
    through to the pl_set_param call, where the error message will be set and
    the appropriate error will be raised.*/
    if (!is_avail(hcam, param_id)) {
        PyErr_SetString(PyExc_AttributeError,
            "Invalid setting for this camera.");
        return NULL;
    }
    if (!pl_set_param(hcam, param_id, &param_value)) {
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
    int16 hcam;
    uns32 param_id;

    /* Build the string to determine the type of the parameter value. */
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "hi", &hcam, &param_id)) {
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
    if (!pl_get_param(hcam, param_id, ATTR_AVAIL, (void *)&avail))
        Py_RETURN_TRUE;

    if (avail)
      Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

/**
 * Collects a single frame and returns it as a numpy array.
 */
static PyObject *
pvc_get_frame(PyObject *self, PyObject *args)
{
    /* TODO: Make setting acquisition apart of this function. Do not make them
       pass into the function call as arguments.
    */
    int16 hcam;    /* Camera handle. */
    uns16 s1;      /* First pixel in serial register. */
    uns16 s2;      /* Last pixel in serial register. */
    uns16 sbin;    /* Serial binning. */
    uns16 p1;      /* First pixel in parallel register. */
    uns16 p2;      /* Last pixel in serial register. */
    uns16 pbin;    /* Parallel binning. */
    uns32 expTime; /* Exposure time. */
    int16 expMode; /* Exposure mode. */
    if (!PyArg_ParseTuple(args, "hhhhhhhih", &hcam, &s1, &s2, &sbin,
        &p1, &p2, &pbin,
        &expTime, &expMode)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    /* Struct that contains the frame size and binning information. */
    rgn_type frame = { s1, s2, sbin, p1, p2, pbin };
    uns32 exposureBytes;

    /* Setup the acquisition. */
    uns16 exp_total = 1;
    uns16 rgn_total = 1;
    if (!pl_exp_setup_seq(hcam, exp_total, rgn_total, &frame, expMode,
            expTime, &exposureBytes)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    /* Since cameras have pixel depths larger than 8 bits (but no larger than
     * 16), we can divide the exposureBytes required for a frame by 2 to
     * determine the size the frame array should be. */
    //printf("%p",g_singleFrameAddress);

    if (g_singleFrameAddress != NULL){
      free(g_singleFrameAddress);
    }
    g_singleFrameAddress =
        new (std::nothrow) uns16[exposureBytes / sizeof(uns16)];
    if (g_singleFrameAddress == NULL) {
        PyErr_SetString(PyExc_MemoryError,
            "Unable to properly allocate memory for frame.");
        return NULL;
    }
    if (!pl_exp_start_seq(hcam, g_singleFrameAddress)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    int16 status;
    uns32 byte_cnt;

    // Keep checking the camera readout status.
    // Function returns FALSE if status is READOUT_FAILED
    while (pl_exp_check_status(hcam, &status, &byte_cnt)
            && status != READOUT_COMPLETE  && status != READOUT_NOT_ACTIVE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (status == READOUT_FAILED) {
        PyErr_SetString(PyExc_RuntimeError, "get_frame() readout failed.");
        return NULL;
    }
    /* A PyArrayObject is a wrapper around the newly captured frame. It
       does NOT reallocate memory for a new list. The number of dimensions,
       the length of each dimension, the type of the data, and a pointer
       to the frame is required to construct the wrapper. */
    import_array();  /* Initialize PyArrayObject. */
    int dimensions = 1;
    npy_intp dimension_lengths = exposureBytes / sizeof(uns16);
    int type = NPY_UINT16;
    PyArrayObject *numpy_frame = (PyArrayObject *)
        PyArray_SimpleNewFromData(dimensions, &dimension_lengths, type,
                                  (void *)g_singleFrameAddress);
    PyArray_Return(numpy_frame);
}


/**
* Collects a sequence of images and returns it as a numpy array.
*/
static PyObject *
pvc_get_sequence(PyObject *self, PyObject *args)
{
	int16 hcam;    /* Camera handle. */
	uns16 exp_total;  /* Number of frames to get */
	uns16 s1;      /* First pixel in serial register. */
	uns16 s2;      /* Last pixel in serial register. */
	uns16 sbin;    /* Serial binning. */
	uns16 p1;      /* First pixel in parallel register. */
	uns16 p2;      /* Last pixel in serial register. */
	uns16 pbin;    /* Parallel binning. */
	uns32 expTime; /* Exposure time. */
	int16 expMode; /* Exposure mode. */
	if (!PyArg_ParseTuple(args, "hhhhhhhhih", &hcam, &exp_total, &s1, &s2, &sbin,
		&p1, &p2, &pbin,
		&expTime, &expMode)) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
		return NULL;
	}
	/* Struct that contains the frame size and binning information. */
	rgn_type frame = { s1, s2, sbin, p1, p2, pbin };
	uns32 exposureBytes;

	/* Setup the acquisition. */
	uns16 rgn_total = 1;
	if (!pl_exp_setup_seq(hcam, exp_total, rgn_total, &frame, expMode,
		expTime, &exposureBytes)) {
		set_g_msg();
		PyErr_SetString(PyExc_RuntimeError, g_msg);
		return NULL;
	}

	/* Allocate buffer for all frames in sequence */
	/*  ONLY IF THE SEQUENCE BUFFER IS GLOBAL?
	if (sequenceBuffer != NULL) {
		free(sequenceBuffer)
	}
	*/
	uns16 *sequenceBuffer =
		new (std::nothrow) uns16[exposureBytes / sizeof(uns16)];
	if (sequenceBuffer == NULL) {
		PyErr_SetString(PyExc_MemoryError,
			"Unable to properly allocate memory for acquisition.");
		return NULL;
	}

	/* Start sequence capture */
	if (!pl_exp_start_seq(hcam, sequenceBuffer)) {
		set_g_msg();
		PyErr_SetString(PyExc_RuntimeError, g_msg);
		return NULL;
	}
	int16 status;
	uns32 byte_cnt;

	// Keep checking the camera readout status.
	// Function returns FALSE if status is READOUT_FAILED
	while (pl_exp_check_status(hcam, &status, &byte_cnt)
		&& status != READOUT_COMPLETE  && status != READOUT_NOT_ACTIVE) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	if (status == READOUT_FAILED) {
		PyErr_SetString(PyExc_RuntimeError, "get_sequence() readout failed.");
		return NULL;
	}

	/* Finish sequence capture (does pl_exp_stop_seq need to be called here?????) */

	/* Wrap the frame buffer into a 1D numpy array to be read and reshaped in python.
	   Does not allocate any additional memory */
	import_array();
	int type = NPY_UINT16;
	int dimensions = 1;
	npy_intp dimension_lengths = exposureBytes / sizeof(uns16);
	PyArrayObject *numpy_frame = (PyArrayObject *)
		PyArray_SimpleNewFromData(dimensions, &dimension_lengths, type,
		(void *)sequenceBuffer);
	/* Tell numpy to free the memory when the array is destroyed */
	PyArray_ENABLEFLAGS((PyArrayObject*)numpy_frame, NPY_ARRAY_OWNDATA);

	PyArray_Return(numpy_frame);
}


static PyObject *
pvc_start_live(PyObject *self, PyObject *args)
{
    SampleContext dataContext;
    //dataContext.myData1 = 0;
    //dataContext.myData2 = 100;

    int16 hcam;    /* Camera handle. */
    uns16 s1;      /* First pixel in serial register. */
    uns16 s2;      /* Last pixel in serial register. */
    uns16 sbin;    /* Serial binning. */
    uns16 p1;      /* First pixel in parallel register. */
    uns16 p2;      /* Last pixel in serial register. */
    uns16 pbin;    /* Parallel binning. */
    uns32 expTime; /* Exposure time. */
    int16 expMode; /* Exposure mode. */
    const int16 bufferMode = CIRC_OVERWRITE;
    const uns16 circBufferFrames = 16;

    if (!PyArg_ParseTuple(args, "hhhhhhhih", &hcam, &s1, &s2, &sbin, &p1, &p2,
                                                    &pbin, &expTime, &expMode)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    if (g_live) {
        PyErr_SetString(PyExc_RuntimeError, "Live sequence already running!");
        return NULL;
    }

    if (PV_OK != pl_cam_register_callback_ex3(hcam, PL_CALLBACK_EOF,
            (void *)NewFrameHandler, (void *)&dataContext))
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to register frame callback EX3.");
            return NULL;
        }
        /* Struct that contains the frame size and binning information. */
        rgn_type frame = { s1, s2, sbin, p1, p2, pbin };
        uns32 exposureBytes;
        g_prevTime = std::chrono::high_resolution_clock::now();
        /* Setup the acquisition. */
        uns16 rgn_total = 1;
        if (!pl_exp_setup_cont(hcam, rgn_total, &frame, expMode,
            expTime, &exposureBytes, bufferMode)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        uns16 *circBufferInMemory =
            new (std::nothrow) uns16[circBufferFrames * exposureBytes / sizeof(uns16)];
        if (circBufferInMemory == NULL) {
            PyErr_SetString(PyExc_MemoryError,
                "Unable to properly allocate memory for frame.");
            return NULL;
        }
        if (!pl_exp_start_cont(hcam, circBufferInMemory, circBufferFrames * exposureBytes / sizeof(uns16))) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        g_live = 1;
    return PyLong_FromLong(exposureBytes);
}

static PyObject *
pvc_get_live_frame(PyObject *self, PyObject *args)
{
	/* TODO: Make setting acquisition apart of this function. Do not make them
	pass into the function call as arguments.
	*/
	int16 hcam;			 /* Camera handle. */
	uns32 exposureBytes; /* Bytes Exposed */

	const int16 bufferMode = CIRC_OVERWRITE;
	const uns16 circBufferFrames = 20;

	if (!PyArg_ParseTuple(args, "hi", &hcam, &exposureBytes)) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
		return NULL;
	}

    if (!g_live) {
		PyErr_SetString(PyExc_RuntimeError, "Camera live feed is not active!");
        return NULL;
    }

	import_array();  /* Initialize PyArrayObject. */
	int dimensions = 1;
	npy_intp dimension_lengths = exposureBytes / sizeof(uns16);
	int type = NPY_UINT16;
	PyObject *numpy_frame = (PyObject *)
		PyArray_SimpleNewFromData(dimensions, &dimension_lengths, type,
		(void *)g_frameAddress);

	PyObject *fps = PyFloat_FromDouble(g_FPS);
	PyObject *tup = PyTuple_New(2);
	PyTuple_SetItem(tup, 0, numpy_frame);
	PyTuple_SetItem(tup, 1, fps);
	return tup;
}

static PyObject *
pvc_stop_live(PyObject *self, PyObject *args)
{
	int16 hcam;    /* Camera handle. */

	if (!PyArg_ParseTuple(args, "h", &hcam)) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
		return NULL;
	}
    if (!g_live) {
        PyErr_SetString(PyExc_RuntimeError, "Live sequence is not running!");
        return NULL;
    }
	if (PV_OK != pl_exp_stop_cont(hcam, CCS_CLEAR)) {	//stop the circular buffer aquisition
		PyErr_SetString(PyExc_ValueError, "Buffer failed to stop");
		return NULL;
	}
    g_live = 0;
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
 *   hcam: The handle of the camera in question.
 *   param_id: The enumerated parameter to check.
 *   selected_val: The value to check if it is a valid selection.
 *
 *  Returns:
 *   0 if selection is not a valid instance of the enumerated type.
 *   1 if selection is a valid instance of the enumerated type.
 */
int valid_enum_param(int16 hcam, uns32 param_id, int32 selected_val)
{
    /* If the enum param is not available return False. */
    rs_bool param_avail = FALSE;
    if (!pl_get_param(hcam, param_id, ATTR_AVAIL, &param_avail)
            || param_avail == FALSE) {
        return 0;
    }
    /* Get the number of valid modes for the setting. */
    uns32 num_selections = 0;
    if (!pl_get_param(hcam, param_id, ATTR_COUNT, &num_selections)) {
        return 0;
    }
    /* Loop over all of the possible selections and see if any match with the
     * selection provided. */
    for (uns32 i=0; i < num_selections; i++) {
        /* Enum name string is required for pl_get_enum_param function. */
        uns32 enum_str_len;
        if (!pl_enum_str_length(hcam, param_id, i, &enum_str_len)) {
            return 0;
        }
        char * enum_str = new char[enum_str_len];
        int32 enum_val;
        if (!pl_get_enum_param(hcam, param_id, i, &enum_val,
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
    int16 hcam;
    uns32 param_id;
    if (!PyArg_ParseTuple(args, "hi", &hcam, &param_id)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    if (!is_avail(hcam, param_id)){
        PyErr_SetString(PyExc_AttributeError, "Invalid setting for camera.");
        return NULL;
    }
    uns32 count;
    if (!pl_get_param(hcam, param_id, ATTR_COUNT, (void *)&count)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }

    PyObject *result = PyDict_New();
    for (uns32 i = 0; i < count; i++) {
        // Get the length of the name of the parameter.
        uns32 str_len;
        if (!pl_enum_str_length(hcam, param_id, i, &str_len)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }

        // Allocate the destination string
        char *name = new (std::nothrow) char[str_len];

        // Get string and value
        int32 value;
        if (!pl_get_enum_param(hcam, param_id, i, &value, name, str_len)) {
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
    int16 hcam;
    /* Parse the arguments provided by the user. */
    if (!PyArg_ParseTuple(args, "h", &hcam)) {
        PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
        return NULL;
    }
    if (!pl_pp_reset(hcam)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    Py_RETURN_NONE;
}

/*
--------------------------- STREAMSAVER ACQUISITION CLASS -------------------------------
 Class to run an acquisition completely within C++. Most efficient image acquisition method.
*/

// Define StreamSaver object
typedef struct {
    PyObject_HEAD
    Helper *helpPtr;
} StreamSaver;

// Initialize StreamSaver object
static int StreamSaver_init(StreamSaver *self, PyObject *args, PyObject *kwargs)
{
    self->helpPtr = new Helper();
    return 0;
}

// Destruct StreamSaver object
static void StreamSaver_dealloc(StreamSaver *self)
{
    delete self->helpPtr;
    Py_TYPE(self)->tp_free(self);
}

static PyObject *StreamSaver_attach_camera(StreamSaver *self, PyObject *args)
{
    // Parse input
	const char *camName; /* Camera name */
	if (!PyArg_ParseTuple(args, "s", &camName)) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
		return NULL;
	}

    // Start Log
	auto consoleLogger = std::make_shared<pm::ConsoleLogger>();
	pm::Log::LogI("Attaching camera!");
    std::string camNameStr(camName);
    if (!(self->helpPtr)->AttachCamera(camNameStr))
	{
		pm::Log::Flush();
		PyErr_SetString(PyExc_RuntimeError, "Could not setup acquisition!!!");
		return NULL;
	}

	pm::Log::Flush();
    Py_RETURN_NONE;
}

static PyObject *StreamSaver_setup_live(StreamSaver *self, PyObject *args)
{
	// Parse input
	uns32 expTime;    /* Exposure time. */
	int16 expMode;    /* Exposure mode. */
	uns16 s1;      /* First pixel in serial register. */
	uns16 s2;      /* Last pixel in serial register. */
	uns16 sbin;    /* Serial binning. */
	uns16 p1;      /* First pixel in parallel register. */
	uns16 p2;      /* Last pixel in serial register. */
	uns16 pbin;    /* Parallel binning. */

	if (!PyArg_ParseTuple(args, "IhHHHHHH", &expTime, &expMode,
		&s1, &s2, &sbin, &p1, &p2, &pbin)) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
		return NULL;
	}
	// Pack ROI's
	std::vector<rgn_type> regions;
	rgn_type rgn;
	rgn.sbin = sbin;
	rgn.pbin = pbin;
	rgn.s1 = s1;
	rgn.s2 = s2;
	rgn.p1 = p1;
	rgn.p2 = p2;
	regions.push_back(rgn);

    // Apply settings
	(self->helpPtr)->SetRegions(regions);
	(self->helpPtr)->SetExposure(expTime);
	(self->helpPtr)->SetAcqMode(pm::AcqMode::LiveCircBuffer);
	(self->helpPtr)->SetStorageType(pm::StorageType::None);
	Py_RETURN_NONE;
}

static PyObject *StreamSaver_setup_acquisition(StreamSaver *self, PyObject *args)
{
	// Parse input
	uns32 expTotal;   /* Number of frames to get */
	uns32 expTime;    /* Exposure time. */
	int16 expMode;    /* Exposure mode. */
	uns16 s1;      /* First pixel in serial register. */
	uns16 s2;      /* Last pixel in serial register. */
	uns16 sbin;    /* Serial binning. */
	uns16 p1;      /* First pixel in parallel register. */
	uns16 p2;      /* Last pixel in serial register. */
	uns16 pbin;    /* Parallel binning. */
	const char *path; /* Output TIFF */

	if (!PyArg_ParseTuple(args, "IIhHHHHHHs", &expTotal, &expTime, &expMode,
		&s1, &s2, &sbin, &p1, &p2, &pbin, &path)) {
		PyErr_SetString(PyExc_ValueError, "Invalid parameters.");
		return NULL;
	}
	// Pack ROI's
	std::vector<rgn_type> regions;
	rgn_type rgn;
	rgn.sbin = sbin;
	rgn.pbin = pbin;
	rgn.s1 = s1;
	rgn.s2 = s2;
	rgn.p1 = p1;
	rgn.p2 = p2;
	regions.push_back(rgn);

    // Apply settings
	(self->helpPtr)->SetRegions(regions);
	(self->helpPtr)->SetAcqFrameCount(expTotal);
	(self->helpPtr)->SetExposure(expTime);
	(self->helpPtr)->SetSaveDir(path);
	(self->helpPtr)->SetAcqMode(pm::AcqMode::SnapCircBuffer);
	(self->helpPtr)->SetStorageType(pm::StorageType::Tiff);
	(self->helpPtr)->SetMaxStackSize(2147483647); // 0xFFFFFFFF / 2 bytes (~2.15 GB)

	Py_RETURN_NONE;
}

static PyObject *StreamSaver_start_acquisition(StreamSaver *self)
{
    // Start Log
	auto consoleLogger = std::make_shared<pm::ConsoleLogger>();
	pm::Log::LogI("Starting Acquisition!");
	pm::Log::LogI("====================\n");

	if (!(self->helpPtr)->InstallTerminationHandlers())
	{
		pm::Log::Flush();
		PyErr_SetString(PyExc_RuntimeError, "Could not install termination handlers!!!");
		return NULL;
	}

	if (!(self->helpPtr)->StartAcquisition())
	{
		pm::Log::Flush();
		PyErr_SetString(PyExc_RuntimeError, "Acquisition start failed!");
		return NULL;
	}

    Py_RETURN_NONE;
}

static PyObject *StreamSaver_join_acquisition(StreamSaver *self)
{
	auto consoleLogger = std::make_shared<pm::ConsoleLogger>();

    Py_BEGIN_ALLOW_THREADS
    if (!(self->helpPtr)->JoinAcquisition())
    {
		pm::Log::Flush();
		PyErr_SetString(PyExc_RuntimeError, "Acquisition join failed!");
		return NULL;
    }
    Py_END_ALLOW_THREADS

    if ((self->helpPtr)->m_userAbortFlag)
    {
		pm::Log::Flush();
		PyErr_SetString(PyExc_RuntimeError, "Acquisition aborted!");
		return NULL;
    }

	pm::Log::LogI("Acquisition exited!");
	pm::Log::Flush();
    Py_RETURN_NONE;
}

static PyObject *StreamSaver_abort_acquisition(StreamSaver *self)
{
    (self->helpPtr)->AbortAcquisition();
    Py_RETURN_NONE;
}

static PyObject *StreamSaver_acquisition_status(StreamSaver *self)
{
    if ((self->helpPtr)->AcquisitionStatus()) Py_RETURN_TRUE;
    else Py_RETURN_FALSE;
}

static PyObject *StreamSaver_input_tick(StreamSaver *self)
{
    (self->helpPtr)->InputTimerTick();
    Py_RETURN_NONE;
}

static PyObject *StreamSaver_acquisition_frame(StreamSaver *self)
{
    void *data;
    uns32 frameBytes = 0;
    uns32 frameNum = 0;
    uns16 frameW = 0;
    uns16 frameH = 0;

    if (!(self->helpPtr)->GetFrameData(&data, &frameBytes, &frameNum, &frameW, &frameH))
    {
	    pm::Log::Flush();
		PyErr_SetString(PyExc_RuntimeError, "Frame is empty/invalid!");
		return NULL;
    }

    // Wrap frame in numpy array
	import_array();
	int type = NPY_UINT16;
	int ndims = 2;
	npy_intp dims[2] = {frameW, frameH};
	PyArrayObject *numpy_frame = (PyArrayObject *)
		PyArray_SimpleNewFromData(ndims, dims, type, data);
	// Tell numpy to free the memory when the array is destroyed
	PyArray_ENABLEFLAGS((PyArrayObject*)numpy_frame, NPY_ARRAY_OWNDATA);
	PyArray_Return(numpy_frame);
}

static PyObject *StreamSaver_acquisition_stats(StreamSaver *self)
{
    double* acqFps = new double;
    size_t* acqFramesValid = new size_t;
    size_t* acqFramesLost = new size_t;
    size_t* acqFramesMax = new size_t;
    size_t* acqFramesCached = new size_t;

    double* diskFps = new double;
    size_t* diskFramesValid = new size_t;
    size_t* diskFramesLost = new size_t;
    size_t* diskFramesMax = new size_t;
    size_t* diskFramesCached = new size_t;

    // Get acquisition related statistics
    if (!(self->helpPtr)->AcquisitionStats(*acqFps, *acqFramesValid, *acqFramesLost,
            *acqFramesMax, *acqFramesCached, *diskFps, *diskFramesValid, *diskFramesLost,
            *diskFramesMax, *diskFramesCached))
    {
		PyErr_SetString(PyExc_RuntimeError, "Acquisition is not active!");
		return NULL;
    }

	PyObject *tup = Py_BuildValue("(dlllldllll)",
            *acqFps, *acqFramesValid, *acqFramesLost, *acqFramesMax, *acqFramesCached,
            *diskFps, *diskFramesValid, *diskFramesLost, *diskFramesMax, *diskFramesCached);
    return tup;
}

static PyMethodDef StreamSaver_methods[] = {
    {"attach_camera",
        (PyCFunction)StreamSaver_attach_camera,
        METH_VARARGS,
        "Attach a camera for the acquisition."},
    {"setup_live",
        (PyCFunction)StreamSaver_setup_live,
        METH_VARARGS,
        "Setup a live capture sequence with a circular buffer. Runs indefinitely."},
    {"setup_acquisition",
        (PyCFunction)StreamSaver_setup_acquisition,
        METH_VARARGS,
        "Setup a capture sequence with a set length and directory to save the frames."},
    {"start_acquisition",
        (PyCFunction)StreamSaver_start_acquisition,
        METH_NOARGS,
        "Start the acquisition. NOTE: attach_camera and apply_options must be "
        "called first!"},
    {"join_acquisition",
        (PyCFunction)StreamSaver_join_acquisition,
        METH_NOARGS,
        "Join the acquisition (wait for completion)."},
    {"abort_acquisition",
        (PyCFunction)StreamSaver_abort_acquisition,
        METH_NOARGS,
        "Signal the acquisition to abort."},
    {"acquisition_status",
        (PyCFunction)StreamSaver_acquisition_status,
        METH_NOARGS,
        "Check status of current acquisition."},
    {"acquisition_stats",
        (PyCFunction)StreamSaver_acquisition_stats,
        METH_NOARGS,
        "Get acquisition and disk thread stats for current acquisition."},
    {"input_tick",
        (PyCFunction)StreamSaver_input_tick,
        METH_NOARGS,
        "Input timer tick to acquisition. Signals the acquisition to cache next "
        "frame."},
    {"acquisition_frame",
        (PyCFunction)StreamSaver_acquisition_frame,
        METH_NOARGS,
        "Get last frame from listener. Frames are only cached after "
        "input_tick has been called. Exactly one frame will be cached "
        "per tick."},
    {NULL} /* Sentinel */
};

static PyTypeObject StreamSaverType = {
    PyVarObject_HEAD_INIT(NULL, 0) "pvc.StreamSaver",  /* tp_name */
    sizeof(StreamSaver),                      /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)StreamSaver_dealloc,          /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_reserved */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash  */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    "StreamSaver class adapted from PVCAM StreamSaving example.", /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    StreamSaver_methods,                      /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)StreamSaver_init,               /* tp_init */
    0,                                        /* tp_alloc */
    PyType_GenericNew,                        /* tp_new */
};

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
	{"get_frame",
		pvc_get_frame,
		METH_VARARGS,
		get_frame_docstring},
	{ "get_sequence",
		pvc_get_sequence,
		METH_VARARGS,
		get_sequence_docstring },
	{"start_live",
		pvc_start_live,
		METH_VARARGS,
		start_live_docstring},
	{ "get_live_frame",
		pvc_get_live_frame,
		METH_VARARGS,
		get_live_frame_docstring },
	{ "stop_live",
		pvc_stop_live,
		METH_VARARGS,
		stop_live_docstring },
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

    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pvcmodule = {
    PyModuleDef_HEAD_INIT,
    "pvc",            // Name of module
    module_docstring, // Module Documentation
    -1,               // Module keeps state in global variables
    PvcMethods,        // Our list of module functions.
};

PyMODINIT_FUNC
PyInit_pvc(void)
{
    Py_Initialize();
    PyObject *m = PyModule_Create(&pvcmodule);

    // Ensure GIL has been created
    if (!PyEval_ThreadsInitialized()) {
        PyEval_InitThreads();
    }

    // Object definitions
    if (PyType_Ready(&StreamSaverType) < 0)
        return NULL;
    Py_INCREF(&StreamSaverType);
    PyModule_AddObject(m, "StreamSaver", (PyObject *)&StreamSaverType);

    return m;
}