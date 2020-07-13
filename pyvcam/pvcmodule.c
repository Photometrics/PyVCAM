#include <Python.h>
#include <master.h>
#include <pvcam.h>


char g_msg[ERROR_MSG_LEN]; // Global Error Message Variable.

static char module_docstring[] =
    "This module provides an interface for various PVCAM functions.";
static char get_pvcam_version_docstring[] =
    "Returns the current version of PVCAM.";
static char get_cam_total_docstring[] =
    "Returns the number of available cameras currently \
     connected to the system.";
static char init_pvcam_docstring[] =
    "Initializes PVCAM library.";
static char uninit_pvcam_docstring[] =
    "Uninitializes PVCAM library.";
static char get_cam_name_docstring[] =
    "Return the name of a camera given its handle/camera number.";
static char open_camera_docstring[] = 
    "Opens a specified camera.";
static char close_camera_docstring[] =
    "Closes a specified camera.";
static char get_param_docstring[] =
    "Returns the value of a camera associated with the specified parameter.";
static char set_param_docstring[] =
    "Sets a specified parameter to a specified value.";
static char get_enum_param_docstring[] =
    "Returns the enumerated value of the specified parameter at `index`.";

/** Sets the global error message. */
void set_g_msg(void) { pl_error_message(pl_error_code(), g_msg); }

/** Returns true if the specified attribute is available. */
int is_avail(hcam, param_id)
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
pvc_get_pvcam_version(PyObject *self)
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

/**
  This function will initialize PVCAM. Must be called before any camera
  interaction may occur.
  @return True if PVCAM successfully initialized.
*/
static PyObject *
pvc_init_pvcam(PyObject *self)
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
pvc_uninit_pvcam(PyObject *self)
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
pvc_get_cam_total(PyObject *self)
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
    if (!PyArg_ParseTuple(args, "i", &cam_num)) {
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
    if (!PyArg_ParseTuple(args, "i", &hcam)) {
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
    if (!PyArg_ParseTuple(args, "iii", &hcam, &param_id, &param_attribute)) {
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
       Otherwise, assume it is a number of some sort (for now). */
    uns16 ret_type;
    if (!pl_get_param(hcam, param_id, ATTR_TYPE, (void *)&ret_type)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    if (ret_type == TYPE_CHAR_PTR) {
        char param_val[MAX_PP_NAME_LEN];
        if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val)) {
            set_g_msg();
            PyErr_SetString(PyExc_RuntimeError, g_msg);
            return NULL;
        }
        return PyUnicode_FromString(param_val);
    }
    int16 param_val;
    if (!pl_get_param(hcam, param_id, param_attribute, (void *)&param_val)) {
        set_g_msg();
        PyErr_SetString(PyExc_RuntimeError, g_msg);
        return NULL;
    }
    return PyLong_FromSize_t(param_val);
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
    if (!PyArg_ParseTuple(args, "iii", &hcam, &param_id, &param_value)) {
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

/* When writing a new function, include it in the Python Method definitions! */
static PyMethodDef PvcMethods[] = {
    {"get_pvcam_version", 
        pvc_get_pvcam_version, 
        METH_NOARGS, 
        get_pvcam_version_docstring},
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
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pvcmodule = {
    PyModuleDef_HEAD_INIT,
    "pvc",            // Name of module
    module_docstring, // Module Documentation
    -1,               // Module keeps state in global variables
    PvcMethods
};

PyMODINIT_FUNC
PyInit_pvc(void)
{
    return PyModule_Create(&pvcmodule);
}
