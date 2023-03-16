"""
driver.py

Wrapper for PyVCAM Camera class to support ARTIQ Network Support Device Package (NDSP) integration into ARTIQ experiment.

Kevin Chen
2023-03-01
QuantumIon
University of Waterloo
"""

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const
from typing import Union, Optional

class PyVCAM:
    """
    Teledyne PrimeBSI camera control.
    """
    WAIT_FOREVER = -1

    def __init__(self) -> None:
        """
        Creates camera object.
        NOTE: This does not open the camera. User has to call open function either in controller or experiment.
        """
        pvc.init_pvcam()
        self.cam = [cam for cam in Camera.detect_camera()][0]

    def __del__(self) -> None:
        """
        Deletes camera object.
        NOTE: This does not close the camera. User has to call close function either in controller or experiment.
        """
        pvc.uninit_pvcam()

    def detect_camera(self) -> NotImplementedError:
        """
        Generator that yields a Camera object for a camera connected to the system.

        NOTE: Function already called in :func:`__init__`. User should not explicitly call this function.

        :raise NotImplementedError: User should not explicitly call this function.
        """
        raise NotImplementedError("Function should not be explicitly called; already called as needed during initialization.")

    def open(self) -> None:
        """
        Opens camera. For more information about how Python interacts with the PVCAM library, refer to pvcmodule.cpp.

        :return: None.
        :raise RuntimeError: If call to PVCAM fails (e.g. Camera is already open).
        """
        self.cam.open()

    def close(self) -> None:
        """
        Closes camera. For more information about how Python interacts with the PVCAM library, refer to pvcmodule.cpp.

        :return: None.
        :raise RuntimeError: If call to PVCAM fails (e.g. Camera is already closed).
        """
        self.cam.close()

    def get_frame(self, exp_time: Optional[int]=None, timeout: Optional[int]=WAIT_FOREVER) -> list[list[int]]:
        """
        Gets a 2D numpy array of pixel data from a single snap image.

        :param exp_time: Exposure time in ms or us. Refer to :func:`exp_res` for the current time unit.
        :param timeout: Duration to wait for new frames in milliseconds. Default set to no timeout (forever).
        :type exp_time: int or None
        :type timeout: int or None

        :return: A 2D np.array containing the pixel data from the captured frame.
        :rtype: list[list[int]]
        """
        return self.cam.get_frame(exp_time, timeout)

    def check_frame_status(self) -> str:
        """
        Gets the frame transfer status. Will raise an exception if called prior to initiating acquisition.

        :return: String representation of PL_IMAGE_STATUSES enum from pvcam.h

            | 'READOUT_NOT_ACTIVE' - The system is @b idle, no data is expected. If any arrives, it will be discarded.
            | 'EXPOSURE_IN_PROGRESS' - The data collection routines are @b active. They are waiting for data to arrive, but none has arrived yet.
            | 'READOUT_IN_PROGRESS' - The data collection routines are @b active. The data has started to arrive.
            | 'READOUT_COMPLETE' - All frames available in sequnece mode.
            | 'FRAME_AVAILABLE' - At least one frame is available in live mode
            | 'READOUT_FAILED' - Something went wrong.
        :rtype: str
        """
        return self.cam.check_frame_status()

    def abort(self) -> None:
        """
        Aborts acquisition.

        :return: None.
        """
        return self.cam.abort()

    def finish(self) -> None:
        """
        Ends a previously started live or sequence acquisition.

        :return: None.
        """
        self.cam.finish()

    def get_param(self, param_id: int, param_attr: Optional[int]=const.ATTR_CURRENT) -> Union[int, float]:
        """
        Gets the current value of a specified parameter. Usually not called directly since the getters/setters
        will handle most cases of getting camera attributes. However, not all cases may be covered by the getters/setters 
        and a direct call may need to be made to PVCAM's get_param function. For more information about how to use get_param, 
        refer to the Using get_param and set_param section of the README for the project.

        :param param_id: The parameter to get. Refer to constants.py for defined constants for each parameter.
        :param param_attr: The desired attribute of the parameter to identify. Refer to constants.py for defined constants for each attribute.
        :type param_id: int
        :type param_attr: int or None

        :return: Value of specified parameter.
        :rtype: int or float
        """
        return self.cam.get_param(param_id, param_attr)

    def set_param(self, param_id: int, value: Union[int, float]) -> None:
        """
        Sets a specified setting of a camera to a specified value. Usually not called directly since the getters/setters
        will handle most cases of getting camera attributes. However, not all cases may be covered by the getters/setters 
        and a direct call may need to be made to PVCAM's get_param function. For more information about how to use get_param, 
        refer to the Using get_param and set_param section of the README for the project.

        :param param_id: An int that corresponds to a camera setting. Refer to constants.py for valid parameter values.
        :param value: The value to set the camera setting to.
        :type param_id: int
        :type value: int or float

        :return: None.
        :raise RuntimeError: If the camera setting cannot be applied.
        :raise ValueError: If the supplied arguments are invalid for the specific parameter.
        """
        self.cam.set_param(param_id, value)

    def bit_depth(self) -> int:
        """
        :return: Bit depth of pixel data for images collected with this camera.
        :rtype: int

        Bit depth cannot be changed directly; instead, users must select a desired speed table index value that has the desired bit depth. 
        Note that a camera may have additional speed table entries for different readout ports. 
        See Port and Speed Choices section inside the PVCAM User Manual for a visual representation of a speed table and to see 
        which settings are controlled by which speed table index is currently selected.
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
        :return: Current exposure mode of the camera.
        :rtype: str

        Refer to :func:`exp_modes` for the available exposure modes.
        Note that exposure modes have names, but PVCAM interprets them as integer values.
        When called as a getter, the integer value will be returned to the user.
        Modified to return more descriptive string.
        """
        return list(self.exp_modes().keys())[list(self.exp_modes().values()).index(self.cam.exp_mode)]

    def set_exp_mode(self, key_or_value: Union[int, str]) -> None:
        """
        Changes exposure mode. Refer to :func:`exp_modes` for the available exposure modes.

        Default exposure modes:

        Keys:
            | 1792 - Internal Trigger
            | 2304 - Edge Trigger
            | 2048 - Trigger first

        :param key_or_value: Key or value to change.

        :return: None.
        :raise ValueError: If provided with an unrecognized key.
        """
        self.cam.exp_mode = key_or_value

    def exp_modes(self) -> dict[str, int]:
        """
        :return: A dictionary containing exposure modes supported by the camera.
        :rtype: dict[str, int]
        """
        return dict(self.cam.exp_modes)

    def get_exp_res(self) -> str:
        """
        :return: Current exposure resolution of a camera.
        :rtype: str

        Refer to :func:`exp_resolutions` for the available exposure resolutions.
        Note that exposure resolutions have names, but PVCAM interprets them as integer values.
        When called as a getter, the integer value will be returned to the user.
        Modified to return more descriptive string.
        """
        return list(self.exp_resolutions().keys())[list(self.exp_resolutions().values()).index(self.cam.exp_res)]

    def set_exp_res(self, key_or_value: Union[int, str]) -> None:
        """
        Changes exposure resolution. Refer to :func:`exp_resolutions` for the available exposure resolutions.

        Default exposure resolutions:

        Keys:
            | 0 - One Millisecond
            | 1 - One Microsecond

        :param key_or_value: Key or value to change.

        :return: None.
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
        :return: The exposure time the camera will use if not given an exposure time.
        :rtype: int

        It is recommended to modify this value to modify your acquisitions for better abstraction.
        """
        return self.cam.exp_time

    def set_exp_time(self, value: int) -> None:
        """
        Changes exposure time in ms or us. Refer to :func:`exp_res` for the current time unit.

        :param value: Desired exposure time in ms or us
        :type value: int

        :return: None.
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

        :return: None.
        """
        self.cam.gain = value

    def last_exp_time(self) -> int:
        """
        :return: The last exposure time the camera used for the last successful 
                non-variable timed mode acquisition in what ever time resolution it was captured at.
        :rtype: int
        """
        return self.cam.last_exp_time

    def pix_time(self) -> int:
        """
        :return: The camera's pixel time, which is the inverse of the speed of the camera.
        :rtype: int

        Pixel time cannot be changed directly; instead users must select a desired speed table index value that has the desired pixel time. 
        Note that a camera may have additional speed table entries for different readout ports. 
        See Port and Speed Choices section inside the PVCAM User Manual for a visual representation of a speed table and to see 
        which settings are controlled by which speed table index is currently selected.
        """
        return self.cam.pix_time

    def port_speed_gain_table(self) -> dict:
        """
        :return: Dictionary containing the port, speed and gain table, which gives information such as 
                bit depth and pixel time for each readout port, speed index and gain.
        :rtype: dict
        """
        return self.cam.port_speed_gain_table

    def get_readout_port(self) -> int:
        """
        Some cameras may have many readout ports, which are output nodes from which a pixel stream can be read from.
        For more information about readout ports, refer to the Port and Speed Choices section inside the PVCAM User Manual

        :return: Current readout port.
        :rtype: int
        """
        return self.cam.readout_port

    def set_readout_port(self, value: int) -> None:
        """
        Changes readout port. By default only one available value (0).

        :param value: Desired readout port.
        :type value: int

        :return: None.
        """
        self.cam.readout_port = value

    def serial_no(self) -> str:
        """
        :return: Camera's serial number.
        :rtype: str
        """
        return self.cam.serial_no

    def get_sequence(self, num_frames: int, exp_time: Optional[int]=None, timeout: Optional[int]=WAIT_FOREVER, interval: Optional[int]=None) -> list[list[list[int]]]:
        """
        Calls the get_frame function in rapid-succession for the specified number of frames.

        :param num_frames: Number of frames to capture in the sequence.
        :param exp_time: The exposure time in ms or us. Refer to :func:`exp_res` for current time unit.
        :param timeout: Duration to wait for new frames in milliseconds. Default set to none (forever).
        :param interval: The time to wait between captures in ms.
        :type num_frames: int
        :type exp_time: int or None
        :type timeout: int or None
        :type interval: int or None

        :return: A 3D np.array containing the pixel data from the captured frames.
        :rtype: list[list[list[int]]]
        """
        return self.cam.get_sequence(num_frames, exp_time, timeout_ms, interval)

    def start_live(self, exp_time: Optional[int]=None, buffer_frame_count: Optional[int]=16, stream_to_disk_path: Optional[str]=None) -> None:
        """
        Sets up a circular buffer acquisition.
        This must be called before poll_frame.

        :param exp_time: The exposure time in ms or us. Refer to :func:`exp_res` for current time unit.
        :param buffer_frame_count: The number of frames in the circular frame buffer.
        :param stream_to_disk_path: The file path for data written directly to disk by PVCAM.
                                    The default is None which disables this feature.
        :type exp_time: int or None
        :type buffer_frame_count: int or None
        :type stream_to_disk_path: str or None

        :return: None.
        """
        self.cam.start_live(exp_time, buffer_frame_count, stream_to_disk_path)

    def start_seq(self, exp_time: Optional[int]=None, num_frames: Optional[int]=1) -> None:
        """
        Sets up a non-circular buffer acquisition.
        This must be called before poll_frame.

        :param exp_time: The exposure time in ms or us. Refer to :func:`exp_res` for current time unit.
        :param num_frames: Total number of frames. Default set to 1.
        :type exp_time: int or None
        :type num_frames: int or None

        :return: None.
        """
        self.cam.start_seq(exp_time, num_frames)

    def poll_frame(self, timeout: Optional[int]=WAIT_FOREVER, oldest_frame: Optional[bool]=True, copy_data: Optional[bool]=True) -> tuple[dict, float, int]:
        """
        Returns a single frame as a dictionary with optional meta data if available. 
        This method must be called after either :func:`start_live` or :func:`start_seq` and before either :func:`abort` or :func:`finish`.
        Pixel data can be accessed via the pixel_data key. Available meta data can be accessed via the meta_data key.

        If multiple ROIs are set, pixel data will be a list of region pixel data of length number of ROIs.
        Meta data will also contain information for each ROI.

        Use :code:`set_param(constants.PARAM_METADATA_ENABLED, True)` to enable meta data.

        :param timeout: Duration to wait for new frames in ms. Default set to no timeout (forever).
        :param oldest_frame: Default True: the returned frame will the oldest frame and will be popped off the queue. 
                            If False, the returned frame will be the newest frame and will not be removed from the queue.
        :param copy_data: Default True: function returns a copy of the numpy frame which points to a new buffer.
                        If False, function returns the original numpy frame which points to the buffer used directly by PVCAM. 
                        Disabling this copy is not recommended for most situations. Refer to PyVCAM Wrapper.md for more details.
        :type timeout: int or None
        :type oldest_frame: bool or None
        :type copy_data: bool or None

        :return: A tuple containing a dictionary with the frame containing available meta data and 2D np.array pixel data, frames per second, and frame count.
        :rtype: tuple[dict, float, int]
        """
        return self.cam.poll_frame(timeout, oldest_frame, copy_data)

    def reset_rois(self) -> None:
        """
        Resets the ROI list to default, which is full frame.

        :return: None.
        """
        self.cam.reset_rois()

    def set_roi(self, s1: int, p1: int, width: int, height: int) -> None:
        """
        Configures a ROI for the camera. The default ROI is the full frame. If the default is
        set or only a single ROI is supported, this function will over-write that ROI. Otherwise,
        this function will attempt to append this ROI to the list.

        :param s1: Serial coordinate 1.
        :param p1: Paralled coordinate 1.
        :param width: Num pixels in serial direction.
        :param height: Num pixels in parallel direction.
        :type s1: int
        :type p1: int
        :type width: int
        :type height: int

        :return: None.
        """
        self.cam.set_roi(s1, p1, width, height)

    def shape(self, roi_index: Optional[int]=0) -> tuple[int, int]:
        """
        Returns the reshape factor to be used when acquiring a ROI. This is equivalent to an acquired image's shape.

        :param roi_index: Index of ROI. Default is 0.
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
        """
        return self.cam.read_enum(param_id)

    def binning(self) -> tuple[int, int]:
        """
        :return: Current serial and parallel binning values.
        :rtype: tuple[int, int]
        """
        return self.cam.binning

    def set_binning(self, value: Union[tuple[int, int], int]) -> None:
        """
        Changes binning. A single value will set a square binning.
        Binning cannot be changed directly on the camera; but is used for setting up
        acquisitions and returning correctly shaped images returned from get_frame and get_live_frame.
        Binning settings for individual ROIs is not supported.

        :param value: Desired binning value as a tuple, or int for a square binning.
        :type value: tuple[int, int] or int

        :return: None.
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
        :return: The last acquisition's post-trigger delay reported by the camera in ms.
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
        :return: The sensor size of the current camera in a tuple in the form (serial sensor size, parallel sensor size).
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
        See the Port and Speed Choices section inside the PVCAM User Manual for
        a detailed explanation about PVCAM speed tables.

        :param value: Desired speed table index.
        :type value: int

        :return: None.
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
        :return: The camera's temperature setpoint in Celsius.
        :rtype: float

        The temperature setpoint is the temperature at which a camera will attempt to stabilize it's temperature (in Celsius) at.
        """
        return self.cam.temp_setpoint

    def set_temp_setpoint(self, value: int) -> None:
        """
        Changes the camera's temperature setpoint.

        :param value: Desired temperature setpoint in Celsius.
        :type value: int

        :return: None.
        """
        self.cam.temp_setpoint = value

    def trigger_table(self) -> dict[str, str]:
        """
        :return: A dictionary containing a table consisting of information of the last acquisition such as exposure time, 
                readout time, clear time, pre-trigger delay, and post-trigger delay. 
                If any of the parameters are unavailable, the dictionary item will be set to 'N/A'.
        :rtype: dict[str, str]
        """
        return self.cam.trigger_table
