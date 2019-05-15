#ifndef PVC_MODULE_H_
#define PVC_MODULE_H_

// System
#include <Python.h>
#include <string>
#include <vector>

#include <cstring>
#include <signal.h>

// PVCAM
#include <master.h>
#include <pvcam.h>

/*
 * Documentation Strings
 */
static char module_docstring[] =
"This module provides an interface for various PVCAM functions.";
static char get_pvcam_version_docstring[] =
"Returns the current version of PVCAM.";
static char get_cam_fw_version_docstring[] =
"Gets the cameras firmware version";
static char get_cam_total_docstring[] =
"Returns the number of available cameras currently connected to the system.";
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
static char check_param_docstring[] =
"Checks if a specified setting of a camera is available.";
static char get_enum_param_docstring[] =
"Returns the enumerated value of the specified parameter at `index`.";
static char get_frame_docstring[] =
"Returns a 2D numpy array of pixel data acquired from a single image.";
static char get_sequence_docstring[] =
"Returns a 3D numpy array of pixel data acquired from a sequence of images.";
static char fast_acquisition_docstring[] =
"Runs an acquisition entirely within C++. Most efficient way to capture long sequence.";
static char start_live_docstring[] =
"Starts live mode aquisiton";
static char start_live_cb_docstring[] =
"Starts live mode aquisiton with callbacks";
static char get_live_frame_docstring[] =
"Gets a frame during life mode";
static char get_live_frame_cb_docstring[] =
"Gets a frame during live mode with callbacks and the fps";
static char stop_live_docstring[] =
"Stops live mode";
static char set_exp_modes_docstring[] =
"Sets a camera's exposure mode or expose out mode.";
static char read_enum_docstring[] =
"Returns a list of all key-value pairs of a given enum type.";
static char reset_pp_docstring[] =
"Resets all post-processing modules to their default values.";

/*
 * Functions
 */
void set_g_msg(void);
int is_avail(int16 hcam, uns32 param_id);
static PyObject *pvc_get_pvcam_version(PyObject *self, PyObject* args);
static PyObject *pvc_get_cam_fw_version(PyObject *self, PyObject* args);
static PyObject *pvc_init_pvcam(PyObject *self, PyObject* args);
static PyObject *pvc_uninit_pvcam(PyObject *self, PyObject* args);
static PyObject *pvc_get_cam_total(PyObject *self, PyObject* args);
static PyObject *pvc_get_cam_name(PyObject *self, PyObject *args);
static PyObject *pvc_open_camera(PyObject *self, PyObject *args);
static PyObject *pvc_close_camera(PyObject *self, PyObject *args);
static PyObject *pvc_get_param(PyObject *self, PyObject *args);
static PyObject *pvc_set_param(PyObject *self, PyObject *args);
static PyObject *pvc_check_param(PyObject *self, PyObject *args);
static PyObject *pvc_get_frame(PyObject *self, PyObject *args);
static PyObject *pvc_get_sequence(PyObject *self, PyObject *args);
static PyObject *pvc_fast_acquisition(PyObject *self, PyObject *args);
static PyObject *pvc_start_live(PyObject *self, PyObject *args);
static PyObject *pvc_get_live_frame(PyObject *self, PyObject *args);
static PyObject *pvc_start_live_cb(PyObject *self, PyObject *args);
static PyObject *pvc_get_live_frame_cb(PyObject *self, PyObject *args);
static PyObject *pvc_stop_live(PyObject *self, PyObject *args);
static PyObject *pvc_set_exp_modes(PyObject *self, PyObject *args);
int valid_enum_param(int16 hcam, uns32 param_id, int32 selected_val);
static PyObject *pvc_reset_pp(PyObject *self, PyObject *args);

#endif // PVC_MODULE_H_
