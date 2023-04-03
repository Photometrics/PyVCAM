"""
driver.py

Wrapper for PyVCAM Camera class to support ARTIQ Network Support Device Package (NDSP) integration
into ARTIQ experiment.

Kevin Chen
2023-03-01
QuantumIon
University of Waterloo
"""

from pyvcam import pvc  # type: ignore
from pyvcam.camera import Camera
from pyvcam import constants as const
from typing import NoReturn


class PyVCAM:
    """
    Teledyne PrimeBSI camera control.
    """
    WAIT_FOREVER = -1  #: Negative values wait forever in functions with :code:`timeout` parameter.

    def __init__(self) -> None:
        """
        Creates camera object.
        **NOTE**: This does not open the camera. User has to call :func:`open` either in controller or experiment.
        """
        pvc.init_pvcam()
        self.cam = [cam for cam in Camera.detect_camera()][0]

    def __del__(self) -> None:
        """
        Deletes camera object.
        **NOTE**: This does not close the camera. User has to call :func:`close` either in controller or experiment.
        """
        pvc.uninit_pvcam()

    def detect_camera(self) -> NoReturn:
        """
        Generator that yields a Camera object for a camera connected to the system.

        **NOTE**: Function already called in :func:`__init__`. User should not explicitly call this function.

        :raise NotImplementedError: User should not explicitly call this function.
        """
        raise NotImplementedError("""Function should not be explicitly called;
                                     already called as needed during initialization.""")

    def open(self) -> None:
        """
        Opens camera.
        For more information about how Python interacts with the PVCAM library, refer to :code:`pvcmodule.cpp`.

        :return: None
        :raise RuntimeError: If call to PVCAM fails (e.g. Camera is already open).
        """
        self.cam.open()

    def close(self) -> None:
        """
        Closes camera.
        For more information about how Python interacts with the PVCAM library, refer to :code:`pvcmodule.cpp`.

        :return: None
        :raise RuntimeError: If call to PVCAM fails (e.g. Camera is already closed).
        """
        self.cam.close()

    def get_frame(self, exp_time: int | None = None, timeout: int | None = WAIT_FOREVER) -> list[list[int]]:
        """
        Gets a 2D numpy array of pixel data from a single snap image.

        :param exp_time: Exposure time in milliseconds or microseconds.
                         Refer to :func:`get_exp_res` for the current exposure time resolution.
        :param timeout: Duration to wait for new frames in milliseconds. Default set to :const:`WAIT_FOREVER`.
        :type exp_time: int or None
        :type timeout: int or None

        :return: A 2D Numpy array containing the pixel data from the captured frame.
        :rtype: list[list[int]]
        :raise ValueError: If invalid parameters are supplied.
        :raise MemoryError: If unable to allocate memory for the camera frame.
        """
        return self.cam.get_frame(exp_time, timeout)

    def check_frame_status(self) -> str:
        """
        Gets the frame transfer status. Will raise an exception if called prior to initiating acquisition.

        :return: String representation of PL_IMAGE_STATUSES enum from :code:`pvcam.h`.

            | 'READOUT_NOT_ACTIVE' - The system is idle, no data is expected. If any arrives, it will be discarded.
            | 'EXPOSURE_IN_PROGRESS' - The data collection routines are active.
                                       They are waiting for data to arrive, but none has arrived yet.
            | 'READOUT_IN_PROGRESS' - The data collection routines are active. The data has started to arrive.
            | 'READOUT_COMPLETE' - All frames available in sequnece mode.
            | 'FRAME_AVAILABLE' - At least one frame is available in live mode
            | 'READOUT_FAILED' - Something went wrong.
        :rtype: str
        """
        return self.cam.check_frame_status()

    def abort(self) -> None:
        """
        Aborts acquisition.

        :return: None
        """
        return self.cam.abort()

    def finish(self) -> None:
        """
        Ends a previously started live or sequence acquisition.

        :return: None
        """
        self.cam.finish()

    def get_param(self, param_id: int, param_attr: int | None = const.ATTR_CURRENT) -> int | float:
        """
        Gets the current value of a specified parameter. Usually not called directly since the getters/setters
        will handle most cases of getting camera attributes. However, not all cases may be covered by the
        getters/setters and a direct call may need to be made to PVCAM's :func:`get_param` function.
        For more information about how to use :func:`get_param`, refer to the
        Using get_param and set_param section of the README for the project.

        :param param_id: The parameter to get. Refer to constants.py for defined constants for each parameter.
        :param param_attr: The desired attribute of the parameter to identify.
                           Refer to constants.py for defined constants for each attribute.
        :type param_id: int
        :type param_attr: int or None

        :return: Value of specified parameter.
        :rtype: int or float
        :raise ValueError: If invalid parameters are supplied.
        :raise AttributeError: If camera does not support the specified parameter.
        """
        return self.cam.get_param(param_id, param_attr)

    def set_param(self, param_id: int, value: int | float) -> None:
        """
        Sets a specified setting of a camera to a specified value. Usually not called directly since the getters/setters
        will handle most cases of getting camera attributes. However, not all cases may be covered by the
        getters/setters and a direct call may need to be made to PVCAM's :func:`get_param` function.
        For more information about how to use :func:`get_param`, refer to the
        Using get_param and set_param section of the README for the project.

        :param param_id: An int that corresponds to a camera setting. Refer to constants.py for valid parameter values.
        :param value: The value to set the camera setting to.
        :type param_id: int
        :type value: int or float

        :return: None
        :raise RuntimeError: If the camera setting cannot be applied.
        :raise ValueError: If the supplied arguments are invalid for the specific parameter.
        :raise AttributeError: If camera does not support the specified parameter.
        """
        self.cam.set_param(param_id, value)

    def check_param(self, param_id: int) -> bool:
        """
        Checks if a specified setting of a camera is available to read/modify.

        :param param_id: An int that corresponds to a camera setting. Refer to constants.py for valid parameter values.
        :type param_id: int

        :return: True if available, False if unavailable.
        :rtype: bool
        """
        return self.cam.check_param(param_id)

    def bit_depth(self) -> int:
        """
        Bit depth cannot be changed directly.
        Instead, users must select a speed table index value that has the desired bit depth.
        Note that a camera may have additional speed table entries for different readout ports.
        See Port and Speed Choices section in the PVCAM User Manual for a visual representation of a
        speed table and to see which settings are controlled by which speed table index is currently selected.

        :return: Bit depth of pixel data for images collected with this camera.
        :rtype: int
        """
        return self.cam.bit_depth

    def cam_fw(self) -> str:
        """
        :return: Current camera firmware version.
        :rtype: str
        """
        return self.cam.cam_fw

    def driver_version(self) -> str:
        """
        :return: A formatted string containing the major, minor, and build version.
        :rtype: str
        """
        return self.cam.driver_version

    def get_exp_mode(self) -> str:
        """
        Refer to :func:`set_exp_mode` for the available exposure modes.

        For a description of exposure modes (Trigger Modes), refer to Prime BSI User Manual page 22.

        :return: Current exposure mode of the camera.
        :rtype: str
        """
        return list(self.exp_modes().keys())[list(self.exp_modes().values()).index(self.cam.exp_mode)]

    def set_exp_mode(self, key_or_value: int | str) -> None:
        """
        Changes exposure mode. Exact case matching is **required**.

        For a description of exposure modes (Trigger Modes), refer to Prime BSI User Manual page 22.

        Default exposure modes:

        Keys:
            | 1792 - Internal Trigger
            | 2304 - Edge Trigger
            | 2048 - Trigger first

        :param key_or_value: Key or value to change.
        :type key_or_value: int or str

        :return: None
        :raise ValueError: If provided with an unrecognized key or value.
        """
        self.cam.exp_mode = key_or_value

    def exp_modes(self) -> dict[str, int]:
        """
        For a description of exposure modes (Trigger Modes), refer to Prime BSI User Manual page 22.

        :return: A dictionary containing exposure modes supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.exp_modes)

    def get_exp_res(self) -> str:
        """
        Refer to :func:`set_exp_res` for the available exposure resolutions.

        :return: Current exposure resolution of a camera.
        :rtype: str
        """
        return list(self.exp_resolutions().keys())[list(self.exp_resolutions().values()).index(self.cam.exp_res)]

    def set_exp_res(self, key_or_value: int | str) -> None:
        """
        Changes exposure resolution. Exact case matching is **required**.

        Default exposure resolutions:

        Keys:
            | 0 - One Millisecond
            | 1 - One Microsecond

        :param key_or_value: Key or value to change.
        :type key_or_value: int or str

        :return: None
        :raise ValueError: If provided with an unrecognized key or value.
        """
        self.cam.exp_res = key_or_value

    def exp_res_index(self) -> int:
        """
        :return: Current exposure resolution index.
        :rtype: int
        """
        return self.cam.exp_res_index

    def exp_resolutions(self) -> dict[str, int]:
        """
        :return: A dictionary containing exposure resolutions supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.exp_resolutions)

    def get_exp_time(self) -> int:
        """
        It is recommended to modify this value to modify your acquisitions for better abstraction.

        :return: The exposure time (in milliseconds or microseconds) the camera will use if not given an exposure time.
                 Refer to :func:`get_exp_res` for the current exposure time resolution.
        :rtype: int
        """
        return self.cam.exp_time

    def set_exp_time(self, value: int) -> None:
        """
        Changes exposure time in milliseconds or microseconds.
        Refer to :func:`get_exp_res` for the current exposure time resolution.

        :param value: Desired exposure time in milliseconds or microseconds.
        :type value: int

        :return: None
        """
        self.cam.exp_time = value

    def get_gain(self) -> int:
        """
        :return: Current gain index for a camera.
        :rtype: int
        :raise ValueError: If an invalid gain index is supplied to the setter.
        """
        return self.cam.gain

    def set_gain(self, value: int) -> None:
        """
        Changes gain.

        :param value: Desired gain value
        :type value: int

        :return: None
        """
        self.cam.gain = value

    def last_exp_time(self) -> int:
        """
        :return: The last exposure time the camera used for the last successful non-variable timed mode acquisition in
                 the time resolution it was captured at.
        :rtype: int
        """
        return self.cam.last_exp_time

    def pix_time(self) -> int:
        """
        Pixel time cannot be changed directly. Instead, users must select a
        speed table index value that has the desired pixel time.
        Note that a camera may have additional speed table entries for different readout ports.
        See Port and Speed Choices section in the PVCAM User Manual for a visual representation of a
        speed table and to see which settings are controlled by which speed table index is currently selected.

        :return: The camera's pixel time, which is the inverse of the speed of the camera.
        :rtype: int
        """
        return self.cam.pix_time

    def port_speed_gain_table(self) -> dict:
        """
        :return: A dictionary containing the port, speed and gain table, which gives information such as
                bit depth and pixel time for each readout port, speed index and gain.
        :rtype: dict
        """
        return self.cam.port_speed_gain_table

    def get_readout_port(self) -> int:
        """
        Some cameras may have many readout ports, which are output nodes from which a pixel stream can be read from.
        For more information about readout ports, refer to the Port and Speed Choices section in the PVCAM User Manual.

        :return: Current readout port.
        :rtype: int
        """
        return self.cam.readout_port

    def set_readout_port(self, value: int) -> None:
        """
        Changes readout port. By default only one available value (0).

        :param value: Desired readout port.
        :type value: int

        :return: None
        """
        self.cam.readout_port = value

    def serial_no(self) -> str:
        """
        :return: Camera's serial number.
        :rtype: str
        """
        return self.cam.serial_no

    def get_sequence(self, num_frames: int, exp_time: int | None = None, timeout: int | None = WAIT_FOREVER,
                     interval: int | None = None) -> list[list[list[int]]]:
        """
        Gets a 3D numpy array of pixel data by calling the :func:`get_frame` function in rapid-succession
        for the specified number of frames.

        :param num_frames: Number of frames to capture in the sequence.
        :param exp_time: The exposure time in milliseconds or microseconds.
                         Refer to :func:`get_exp_res` for the current exposure time resolution.
        :param timeout: Duration to wait for new frames in milliseconds. Default set to :const:`WAIT_FOREVER`.
        :param interval: The time to wait between captures in milliseconds. Default set to 0.
        :type num_frames: int
        :type exp_time: int or None
        :type timeout: int or None
        :type interval: int or None

        :return: A 3D Numpy array containing the pixel data from the captured frames.
        :rtype: list[list[list[int]]]
        :raise ValueError: If invalid parameters are supplied.
        :raise MemoryError: If unable to allocate memory for the camera frame(s).
        """
        return self.cam.get_sequence(num_frames, exp_time, timeout, interval)

    def get_vtm_sequence(self, time_list: list[int], exp_res: int, num_frames: int, timeout: int | None = WAIT_FOREVER,
                         interval: int | None = None) -> list[list[list[int]]]:
        """
        A modified :func:`get_sequence` function to be used for Variable Timed Mode.
        Before calling it, set the camera's exposure mode to "Variable Timed".
        The timings will always start at the first given and keep looping around until it has
        captured the number of frames specified. Multiple ROIs are not supported.

        :param time_list: The timings to be used by the camera in Variable Timed Mode.
        :param exp_res: The exposure time resolution. Currently only supports milliseconds (0) and microseconds (1).
        :param num_frames: The number of frames to be captured in the sequence.
        :param timeout: Duration to wait for new frames in milliseconds. Default set to :const:`WAIT_FOREVER`.
        :param interval: The time to wait between each sequence frame in milliseconds. Default set to 0.
        :type time_list: list[int]
        :type exp_res: int
        :type num_frames: int
        :type timeout: int or None
        :type interval: int or None

        :return: A 3D Numpy array containing the pixel data from the captured sequence.
        :rtype: list[list[list[int]]]
        :raise ValueError: If invalid parameters are supplied.
        :raise MemoryError: If unable to allocate memory for the camera frame(s).
        """
        return self.cam.get_vtm_sequence(time_list, exp_res, num_frames, timeout, interval)

    def start_live(self, exp_time: int | None = None, buffer_frame_count: int | None = 16,
                   stream_to_disk_path: str | None = None) -> None:
        """
        Sets up a circular buffer acquisition.
        This must be called before :func:`poll_frame`.

        :param exp_time: The exposure time in milliseconds or microseconds.
                         Refer to :func:`get_exp_res` for the current exposure time resolution.
        :param buffer_frame_count: The number of frames in the circular frame buffer. Default set to 16.
        :param stream_to_disk_path: The file path for data written directly to disk by PVCAM.
                                    Default set to :code:`None` which disables this feature.
        :type exp_time: int or None
        :type buffer_frame_count: int or None
        :type stream_to_disk_path: str or None

        :return: None
        """
        self.cam.start_live(exp_time, buffer_frame_count, stream_to_disk_path)

    def start_seq(self, exp_time: int | None = None, num_frames: int | None = 1) -> None:
        """
        Sets up a non-circular buffer acquisition.
        This must be called before :func:`poll_frame`.

        :param exp_time: The exposure time in milliseconds or microseconds.
                         Refer to :func:`get_exp_res` for the current exposure time resolution.
        :param num_frames: Total number of frames. Default set to 1.
        :type exp_time: int or None
        :type num_frames: int or None

        :return: None
        """
        self.cam.start_seq(exp_time, num_frames)

    def poll_frame(self, timeout: int | None = WAIT_FOREVER, oldest_frame: bool | None = True,
                   copy_data: bool | None = True) -> tuple[dict, float, int]:
        """
        Returns a single frame as a dictionary with optional meta data if available.
        This method must be called after either :func:`start_live` or :func:`start_seq` and
        before either :func:`abort` or :func:`finish`.

        If multiple ROIs are set, pixel data will be a list of region pixel data of length number of ROIs.
        Meta data will also contain information for each ROI.

        Use :code:`set_param(constants.PARAM_METADATA_ENABLED, True)` to enable meta data.

        :param timeout: Duration to wait for new frames in milliseconds. Default set to :const:`WAIT_FOREVER`.
        :param oldest_frame: Default True: the returned frame will the oldest frame and will be popped off the queue.
                        If False, the returned frame will be the newest frame and will not be removed from the queue.
        :param copy_data: Default True: function returns a copy of the numpy frame which points to a new buffer.
                If False, function returns the original numpy frame which points to the buffer used directly by PVCAM.
                Disabling this copy is not recommended for most situations. Refer to PyVCAM Wrapper.md for more details.
        :type timeout: int or None
        :type oldest_frame: bool or None
        :type copy_data: bool or None

        :return: A tuple containing a dictionary with the frame containing
                 available meta data and 2D np.array pixel data, frames per second, and frame count.
        :rtype: tuple[dict, float, int]
        """
        return self.cam.poll_frame(timeout, oldest_frame, copy_data)

    def reset_rois(self) -> None:
        """
        Resets the ROI list to default, which is full frame.

        For a description of ROIs, refer to Prime BSI User Manual page 18.

        :return: None
        """
        self.cam.reset_rois()

    def set_roi(self, s1: int, p1: int, width: int, height: int) -> None:
        """
        Configures a ROI for the camera. The default ROI is the full frame. If the default is
        set or only a single ROI is supported, this function will over-write that ROI. Otherwise,
        this function will attempt to append this ROI to the list.

        For a description of ROIs, refer to Prime BSI User Manual page 18.

        :param s1: Serial coordinate 1.
        :param p1: Paralled coordinate 1.
        :param width: Number of pixels in the serial direction.
        :param height: Number of pixels in the parallel direction.
        :type s1: int
        :type p1: int
        :type width: int
        :type height: int

        :return: None
        """
        self.cam.set_roi(s1, p1, width, height)

    def shape(self, roi_index: int | None = 0) -> tuple[int, int]:
        """
        Returns the reshape factor to be used when acquiring a ROI. This is equivalent to an acquired image's shape.

        :param roi_index: Index of ROI. Default set to 0.
        :type roi_index: int or None

        :return: Reshape factor to be used.
        :rtype: tuple[int, int]
        """
        return self.cam.shape(roi_index)

    def read_enum(self, param_id: int) -> dict[str, int]:
        """
        Returns all settings names paired with their values of a parameter.

        :param param_id: The parameter ID.
        :type param_id: int

        :return: A dictionary containing strings mapped to values.
        :rtype: dict[str, int]
        :raise ValueError: If invalid parameters are supplied.
        :raise AttributeError: If an invalid setting for the camera is supplied.
        """
        return self.cam.read_enum(param_id)

    def binning(self) -> tuple[int, int]:
        """
        :return: Current serial and parallel binning values.
        :rtype: tuple[int, int]
        """
        return self.cam.binning

    def set_binning(self, value: tuple[int, int] | int) -> None:
        """
        Changes binning. A single value will set a square binning.
        Binning cannot be changed directly on the camera, but is used for setting up
        acquisitions and returning correctly shaped images returned from :func:`get_frame` and :func:`get_live_frame`.
        Binning settings for individual ROIs is not supported.

        :param value: Desired binning value(s) as a tuple, or an int for a square binning.
        :type value: tuple[int, int] or int

        :return: None
        """
        self.cam.binning = value

    def chip_name(self) -> str:
        """
        :return: The camera sensor's name.
        :rtype: str
        """
        return self.cam.chip_name

    def post_trigger_delay(self) -> int:
        """
        :return: The last acquisition's post-trigger delay reported by the camera in milliseconds.
        :rtype: int
        """
        return self.cam.post_trigger_delay

    def pre_trigger_delay(self) -> int:
        """
        :return: The last acquisition's pre-trigger delay reported by the camera in milliseconds.
        :rtype: int
        """
        return self.cam.pre_trigger_delay

    def scan_line_time(self) -> int:
        """
        :return: The scan line time in nanoseconds.
        :rtype: int
        """
        return self.cam.scan_line_time

    def sensor_size(self) -> tuple[int, int]:
        """
        :return: The sensor size of the camera in a tuple in the form (serial sensor size, parallel sensor size).
        :rtype: tuple[int, int]
        """
        return self.cam.sensor_size

    def get_speed_table_index(self) -> int:
        """
        :return: The current numerical index of the speed table of the camera.
        :rtype: int
        """
        return self.cam.speed_table_index

    def set_speed_table_index(self, value: int) -> None:
        """
        Changes the current numerical index of the speed table of the camera.
        See the Port and Speed Choices section in the PVCAM User Manual for
        a detailed explanation about PVCAM speed tables.

        :param value: Desired speed table index.
        :type value: int

        :return: None
        """
        self.cam.speed_table_index = value

    def temp(self) -> float:
        """
        :return: Current temperature of a camera in Celsius.
        :rtype: float
        """
        return self.cam.temp

    def get_temp_setpoint(self) -> float:
        """
        The temperature setpoint is the temperature at which a camera will attempt to stabilize its temperature.

        :return: The camera's temperature setpoint in Celsius.
        :rtype: float
        """
        return self.cam.temp_setpoint

    def set_temp_setpoint(self, value: int) -> None:
        """
        Changes the camera's temperature setpoint.

        :param value: Desired temperature setpoint in Celsius.
        :type value: int

        :return: None
        """
        self.cam.temp_setpoint = value

    def trigger_table(self) -> dict[str, str]:
        """
        :return: A dictionary containing a table consisting of information of the last acquisition such as
                exposure time, readout time, clear time, pre-trigger delay, and post-trigger delay.
                If any of the parameters are unavailable, the dictionary item will be set to 'N/A'.
        :rtype: dict[str, str]
        """
        return self.cam.trigger_table

    def reset_pp(self) -> None:
        """
        Resets the post-processing settings to default.

        :return: None
        """
        self.cam.reset_pp()

    def sw_trigger(self) -> None:
        """
        Performs a software trigger. This trigger behaves analogously to an external (hardware) trigger.

        :return: None
        """
        self.cam.sw_trigger()

    def get_post_processing_param(self, feature_name: str, param_name: str) -> int:
        """
        Gets the current value of a post processing parameter.

        :param feature_name: A string name for the post-processing feature using this parameter.
                             Use :func:`post_processing_table` for feature names.
        :param param_name: A string name for the post-processing parameter.
                           Use :func:`post_processing_table` for parameter names.
        :type feature_name: str
        :type param_name: str

        :return: Value of specified post processing parameter.
        :rtype: int
        """
        return self.cam.get_post_processing_param(feature_name, param_name)

    def set_post_processing_param(self, feature_name: str, param_name: str, value: int) -> None:
        """
        Sets the value of a specified post-processing parameter.

        :param feature_name: A string name for the post-processing feature using this parameter.
                             Use :func:`post_processing_table` for feature names.
        :param param_name: A string name for the post-processing parameter.
                           Use :func:`post_processing_table` for parameter names.
        :param value: The value to be assigned to the post-processing parameter.
                      Value must fall within the range provided by :func:`post_processing_table`.
        :type feature_name: str
        :type param_name: str
        :type value: str

        :return: None
        """
        self.cam.set_post_processing_param(feature_name, param_name, value)

    def handle(self) -> int:
        """
        Returns the camera's handle. Note that this is a read only variable, meaning it should never be modified.
        PVCAM will handle the updating of this attribute when the camera is opened or closed.

        :return: The camera's handle.
        :rtype: int
        """
        return self.cam.handle

    def is_open(self) -> bool:
        """
        Returns if the camera is open. Note that this is a read only variable, meaning it should never be modified.
        PVCAM will handle the updating of this attribute when the camera is opened or closed.

        :return: True if the camera is currently open, False otherwise.
        :rtype: bool
        """
        return self.cam.is_open

    def name(self) -> str:
        """
        Returns the camera's name. Note that this is a read only variable, meaning it should never be modified.
        PVCAM will handle the updating of this attribute when the camera is opened or closed.

        :return: The camera's name.
        :rtype: str
        """
        return self.cam.name

    def post_processing_table(self) -> dict:
        """
        :return: A dictionary of the post-processing parameters and their current values of the camera.
        :rtype: dict
        """
        return self.cam.post_processing_table

    def centroids_modes(self) -> dict[str, int]:
        """
        :return: A dictionary containing centroid modes supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.centroids_modes)

    def get_centroids_mode(self) -> str:
        """
        Attempts to read the current centroid mode, which is not supported by the Prime BSI Camera.

        :raise AttributeError: This function is not supported by the Prime BSI Camera.
        """
        return list(self.centroids_modes().keys())[list(self.centroids_modes().values()).index(self.cam.centroids_mode)]

    def set_centroids_mode(self, key_or_value: int | str) -> None:
        """
        Attempts to modify the current centroid mode, which is not supported by the Prime BSI Camera.

        :raise AttributeError: This function is not supported by the Prime BSI Camera.
        """
        self.cam.centroids_mode = key_or_value

    def clear_modes(self) -> dict[str, int]:
        """
        For a description of clear modes, refer to Prime BSI User Manual page 18.

        :return: A dictionary containing clear modes supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.clear_modes)

    def get_clear_mode(self) -> str:
        """
        Refer to :func:`set_clear_mode` for the available clear modes.

        For a description of clear modes, refer to Prime BSI User Manual page 18.

        :return: Current clear mode of a camera.
        :rtype: str
        """
        return list(self.clear_modes().keys())[list(self.clear_modes().values()).index(self.cam.clear_mode)]

    def set_clear_mode(self, key_or_value: int | str) -> None:
        """
        Changes clear mode. Exact case matching is **required**.

        For a description of clear modes, refer to Prime BSI User Manual page 18.

        Default clear modes:

        Keys:
            | 0 - Auto

        :param key_or_value: Key or value to change.
        :type key_or_value: int or str

        :return: None
        :raise ValueError: If provided with an unrecognized key or value.
        """
        self.cam.clear_mode = key_or_value

    def exp_out_modes(self) -> dict[str, int]:
        """
        For a description of Expose Out modes, refer to Prime BSI User Manual page 23.

        :return: A dictionary containing exposure out modes supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.exp_out_modes)

    def get_exp_out_mode(self) -> str:
        """
        Refer to :func:`set_exp_out_mode` for the available exposure out modes.

        For a description of Expose Out modes, refer to Prime BSI User Manual page 23.

        :return: Current exposure out mode of a camera.
        :rtype: str
        """
        return list(self.exp_out_modes().keys())[list(self.exp_out_modes().values()).index(self.cam.exp_out_mode)]

    def set_exp_out_mode(self, key_or_value: int | str) -> None:
        """
        Changes exposure out mode. Exact case matching is **required**.

        For a description of Expose Out modes, refer to Prime BSI User Manual page 23.

        Default exposure out modes:

        Keys:
            | 0 - First Row
            | 2 - Any Row
            | 3 - Rolling Shutter
            | 4 - Line Output

        :param key_or_value: Key or value to change.
        :type key_or_value: int or str

        :return: None
        :raise ValueError: If provided with an unrecognized key or value.
        """
        self.cam.exp_out_mode = key_or_value

    def prog_scan_modes(self) -> dict[str, int]:
        """
        For a description of Programmable Scan Modes, refer to Prime BSI User Manual page 19.

        :return: A dictionary containing programmable scan modes supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.prog_scan_modes)

    def get_prog_scan_mode(self) -> str:
        """
        Refer to :func:`set_prog_scan_mode` for the available programmable scan modes.

        For a description of Programmable Scan Modes, refer to Prime BSI User Manual page 19.

        :return: Current programmable scan mode of a camera.
        :rtype: str
        """
        return list(self.prog_scan_modes().keys())[list(self.prog_scan_modes().values()).index(self.cam.prog_scan_mode)]

    def set_prog_scan_mode(self, key_or_value: int | str) -> None:
        """
        Changes programmable scan mode. Exact case matching is **required**.

        For a description of Programmable Scan Modes, refer to Prime BSI User Manual page 19.

        Default programmable scan modes:

        Keys:
            | 0 - Auto
            | 1 - Line Delay
            | 2 - Scan Width

        :param key_or_value: Key or value to change.
        :type key_or_value: int or str

        :return: None
        :raise ValueError: If provided with an unrecognized key or value.
        """
        self.cam.prog_scan_mode = key_or_value

    def prog_scan_dirs(self) -> dict[str, int]:
        """
        :return: A dictionary containing programmable scan directions supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.prog_scan_dirs)

    def get_prog_scan_dir(self) -> str:
        """
        Refer to :func:`set_prog_scan_dir` for the available programmable scan directions.

        :return: Current programmable scan direction of a camera.
        :rtype: str
        """
        return list(self.prog_scan_dirs().keys())[list(self.prog_scan_dirs().values()).index(self.cam.prog_scan_dir)]

    def set_prog_scan_dir(self, key_or_value: int | str) -> None:
        """
        Changes programmable scan mode. Exact case matching is **required**.

        Default programmable scan directions:

        Keys:
            | 0 - Down
            | 1 - Up
            | 2 - Down/Up Alternate

        :param key_or_value: Key or value to change.
        :type key_or_value: int or str

        :return: None
        :raise ValueError: If provided with an unrecognized key or value.
        """
        self.cam.prog_scan_dir = key_or_value

    def get_prog_scan_dir_reset(self) -> bool:
        """
        :return: The scan direction reset state of camera. The parameter is used with
                 alternate scan directions (down-up) to reset the direction with every acquisition.
        :rtype: bool
        """
        return self.cam.prog_scan_dir_reset

    def set_prog_scan_dir_reset(self, value: bool) -> None:
        """
        Changes scan direction reset state of camera.
        The parameter is used with alternate scan directions (down-up) to reset the direction with every acquisition.

        :return: None
        """
        self.cam.prog_scan_dir_reset = value

    def get_prog_scan_line_delay(self) -> int:
        """
        :return: The scan line delay. The parameter access mode depends on the current programmable scan mode
                 returned by :func:`get_prog_scan_mode`.
        :rtype: int
        """
        return self.cam.prog_scan_line_delay

    def set_prog_scan_line_delay(self, value: int) -> None:
        """
        Changes the scan line delay.
        The parameter access mode depends on the current programmable scan mode returned by :func:`get_prog_scan_mode`.

        :return: None
        """
        self.cam.prog_scan_line_delay = value

    def get_prog_scan_width(self) -> int:
        """
        :return: The scan width. The parameter access mode depends on the current programmable scan mode
                 returned by :func:`get_prog_scan_mode`.
        :rtype: int
        """
        return self.cam.prog_scan_width

    def set_prog_scan_width(self, value: int) -> None:
        """
        Changes the scan width.
        The parameter access mode depends on the current programmable scan mode returned by :func:`get_prog_scan_mode`.

        :return: None
        """
        self.cam.prog_scan_width = value

    def adc_offset(self) -> int:
        """
        :return: The camera's current ADC offset value. Only CCD camera's have ADCs (analog-to-digital converters).
        :rtype: int
        """
        return self.cam.adc_offset

    def get_vtm_exp_time(self) -> int:
        """
        :return: The variable timed exposure time the camera uses for the "Variable Timed" exposure mode.
        :rtype: int
        """
        return self.cam.vtm_exp_time

    def set_vtm_exp_time(self, value: int) -> None:
        """
        Changes the variable timed exposure time the camera uses for the "Variable Timed" exposure mode.

        :return: None
        """
        self.cam.vtm_exp_time = value

    def readout_time(self) -> int:
        """
        :return: The last acquisition's readout time as reported by the camera in microseconds.
        :rtype: int
        """
        return self.cam.readout_time

    def clear_time(self) -> int:
        """
        :return: The last acquisition's clearing time as reported by the camera in microseconds.
        :rtype: int
        :raise RuntimeError: If acquisition setup is not done before calling this function.
        """
        return self.cam.clear_time

    def get_meta_data_enabled(self) -> bool:
        """
        :return: True if meta data is enabled, False otherwise.
        :rtype: bool
        """
        return self.cam.meta_data_enabled

    def set_meta_data_enabled(self, value: bool) -> None:
        """
        Changes meta data setting.

        :return: None
        """
        self.cam.meta_data_enabled = value
