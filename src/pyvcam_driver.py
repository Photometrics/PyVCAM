from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const
from typing import Union

class PyVCAM:
    """
    Teledyne PrimeBSI camera control.
    """
    WAIT_FOREVER = -1

    def __init__(self) -> None:
        """
        Creates camera object.
        NOTE: This does not open the camera. User has to call open function.
        """
        pvc.init_pvcam()
        self.cam = [cam for cam in Camera.detect_camera()][0]

    def detect_camera(self) -> NotImplementedError:
        """
        Generator that yields a Camera object for a camera connected to the system.
        NOTE: Function already called in __init__(). User should not explicitly call this function.

        :raises NotImplementedError:
        """
        raise NotImplementedError("Function should not be explicitly called; already implemented during initialization.")

    def open(self) -> None:
        """
        Opens camera. A RuntimeError will be raised if the call to PVCAM fails. 
        For more information about how Python interacts with the PVCAM library, refer to pvcmodule.cpp.

        :return: None.
        """
        self.cam.open()

    def close(self) -> None:
        """
        Closes camera. Automatically called during uninitialization.
        A RuntimeError will be raised if the call to PVCAM fails. 
        For more information about how Python interacts with the PVCAM library, refer to pvcmodule.cpp.

        :return: None.
        """
        self.cam.close()
    
    def get_frame(self, exp_time: int=None, timeout_ms: int=WAIT_FOREVER) -> list[list[int]]:
        """
        Calls the pvc.get_frame function with the current camera settings to get a 2D numpy array of pixel data from a single snap image.

        :param exp_time: Exposure time.
        :param timeout_ms: Duration to wait for new frames. Default set to no timeout (forever).
        :return: A 2D np.array containing the pixel data from the captured frame.
        """
        return self.cam.get_frame(exp_time, timeout_ms)

    def check_frame_status(self) -> str:
        """
        Gets the frame transfer status. Will raise an exception if called prior to initiating acquisition

        :return: String representation of PL_IMAGE_STATUSES enum from pvcam.h
                'READOUT_NOT_ACTIVE' - The system is @b idle, no data is expected. If any arrives, it will be discarded.
                'EXPOSURE_IN_PROGRESS' - The data collection routines are @b active. They are waiting for data to arrive, but none has arrived yet.
                'READOUT_IN_PROGRESS' - The data collection routines are @b active. The data has started to arrive.
                'READOUT_COMPLETE' - All frames available in sequnece mode.
                'FRAME_AVAILABLE' - At least one frame is available in live mode
                'READOUT_FAILED' - Something went wrong.
        """
        return self.cam.check_frame_status()

    def abort(self) -> None:
        """
        Calls the pvc.abort function that aborts acquisition.

        :return: None.
        """
        return self.cam.abort()

    def finish(self) -> None:
        """
        Ends a previously started live or sequence acquisition.

        :return: None.
        """
        self.cam.finish()
    
    def get_param(self, param_id: int, param_attr: int=const.ATTR_CURRENT) -> int:
        """
        Gets the current value of a specified parameter.

        :param param_id: The parameter to get. Refer to constants.py for
                            defined constants for each parameter.
        :param param_attr: The desired attribute of the parameter to
                            identify. Refer to constants.py for defined
                            constants for each attribute.
        :return: Value of specified parameter.
        """
        return self.cam.get_param(param_id, param_attr)

    def set_param(self, param_id: int, value: int) -> None:
        """
        Sets a specified setting of a camera to a specified value.

        Note that pvc will raise a RuntimeError if the camera setting can not be
        applied. Pvc will also raise a ValueError if the supplied arguments are
        invalid for the specified parameter.

        :param param_id: An int that corresponds to a camera setting. Refer to
                            constants.py for valid parameter values.
        :param value: The value to set the camera setting to.
        :return: None.
        """
        self.cam.set_param(param_id, value)

    def bit_depth(self) -> int:
        """
        Returns the bit depth of pixel data for images collected with this camera. 
        Bit depth cannot be changed directly; instead, users must select a desired speed table index value that has the desired bit depth. 
        Note that a camera may have additional speed table entries for different readout ports. 
        See Port and Speed Choices section inside the PVCAM User Manual for a visual representation of a speed table and to see which 
        settings are controlled by which speed table index is currently selected.

        :return:
        """
        return self.cam.bit_depth

    def cam_fw(self) -> str:
        """
        Returns current camera firmware version as a string.

        :return:
        """
        return self.cam.cam_fw

    def driver_version(self) -> str:
        """
        Returns a formatted string containing the major, minor, and build version.

        :return:
        """
        return self.cam.driver_version

    def exp_mode(self) -> str:
        """
        Returns the current exposure mode of the camera. 
        Note that exposure modes have names, but PVCAM interprets them as integer values. 
        When called as a getter, the integer value will be returned to the user.
        Modified to return more descriptive string.

        :return:
        """
        return list(self.exp_modes().keys())[list(self.exp_modes().values()).index(self.cam.exp_mode)]

    def set_exp_mode(self, key_or_value: int) -> None:
        """
        Changes exposure mode.
        Will raise ValueError if provided with an unrecognized key.
        Keys:
            1792 - Internal Trigger
            2304 - Edge Trigger
            2048 - Trigger first

        :param key_or_value: Key or value to change.
        :return: None.
        """
        self.cam.exp_mode = key_or_value

    def exp_modes(self) -> dict[str, int]:
        """
        Returns dict containing exposure modes supported by the camera.

        :return:
        """
        return dict(self.cam.exp_modes)

    def exp_res(self) -> str:
        """
        Returns the current exposure resolution of a camera. 
        Note that exposure resolutions have names, but PVCAM interprets them as integer values. 
        When called as a getter, the integer value will be returned to the user.
        Modified to return more descriptive string.

        :return:
        """
        return list(self.exp_resolutions().keys())[list(self.exp_resolutions().values()).index(self.cam.exp_res)]

    def set_exp_res(self, key_or_value: int) -> None:
        """
        Changes exposure resolution.
        Keys:
            0 - One Millisecond
            1 - One Microsecond

        :param key_or_value: Key or value to change.
        :return: None.
        """
        self.cam.exp_res = key_or_value

    def exp_res_index(self) -> int:
        """
        Returns current exposure resolution index.

        :return:
        """
        return self.cam.exp_res_index

    def exp_resolutions(self) -> dict[str, int]:
        """
        Returns dict containing exposure resolutions supported by the camera.

        :return:
        """
        return dict(self.cam.exp_resolutions)

    def exp_time(self) -> int:
        """
        Returns the exposure time the camera will use if not given an exposure time. 
        It is recommended to modify this value to modify your acquisitions for better abstraction.

        :return:
        """
        return self.cam.exp_time
    
    def set_exp_time(self, value: int) -> None:
        """
        Changes exposure time.

        :param value: Desired exposure time
        :return: None.
        """
        self.cam.exp_time = value

    def gain(self) -> int:
        """
        Returns the current gain index for a camera. 
        A ValueError will be raised if an invalid gain index is supplied to the setter.

        :return:
        """
        return self.cam.gain

    def set_gain(self, value: int) -> None:
        """
        Changes gain.

        :param value: Desired gain value
        :return: None.
        """
        self.cam.gain = value

    def last_exp_time(self) -> int:
        """
        Returns the last exposure time the camera used for the last successful 
        non-variable timed mode acquisition in what ever time resolution it was captured at.

        :return:
        """
        return self.cam.last_exp_time

    def pix_time(self) -> int:
        """
        Returns the camera's pixel time, which is the inverse of the speed of the camera. 
        Pixel time cannot be changed directly; instead users must select a desired speed table index value that has the desired pixel time. 
        Note that a camera may have additional speed table entries for different readout ports. 
        See Port and Speed Choices section inside the PVCAM User Manual for a visual representation of a speed table and to see which 
        settings are controlled by which speed table index is currently selected.

        :return:
        """
        return self.cam.pix_time

    def port_speed_gain_table(self) -> dict:
        """
        Returns a dictionary containing the port, speed and gain table, 
        which gives information such as bit depth and pixel time for each readout port, speed index and gain.

        :return:
        """
        return self.cam.port_speed_gain_table

    def readout_port(self) -> int:
        """
        Some cameras may have many readout ports, which are output nodes from which a pixel stream can be read from. 
        For more information about readout ports, refer to the Port and Speed Choices section inside the PVCAM User Manual

        :return:
        """
        return self.cam.readout_port

    def set_readout_port(self, value: int) -> None:
        """
        Changes readout port. By default only one available value (0).

        :param value: Desired readout port.
        :return: None.
        """
        self.cam.readout_port = value

    def serial_no(self) -> str:
        """
        Returns the camera's serial number as a string.

        :return:
        """
        return self.cam.serial_no
    
    def get_sequence(self, num_frames: int, exp_time: int=None, 
                     timeout_ms: int=WAIT_FOREVER, interval: int=None) -> list[list[list[int]]]:
        """
        Calls the pvc.get_frame function with the current camera settings in
        rapid-succession for the specified number of frames

        :param num_frames: Number of frames to capture in the sequence.
        :param exp_time: The exposure time.
        :param interval: The time in milliseconds to wait between captures.

        :return: A 3D np.array containing the pixel data from the captured frames.
        """
        return self.cam.get_sequence(num_frames, exp_time, timeout_ms, interval)
    
    def start_live(self, exp_time: int=None, buffer_frame_count: int=16,
                   stream_to_disk_path: str=None) -> None:
        """
        Calls the pvc.start_live function to setup a circular buffer acquisition.

        :param exp_time: The exposure time.
        :param buffer_frame_count: The number of frames in the circular frame buffer.
        :param stream_to_disk_path: The file path for data written directly to disk by PVCAM. 
                                    The default is None which disables this feature.

        :return: None.
        """
        self.cam.start_live(exp_time, buffer_frame_count, stream_to_disk_path)
    
    def start_seq(self, exp_time: int=None, num_frames: int=1) -> None:
        """
        Calls the pvc.start_seq function to setup a non-circular buffer acquisition.
        This must be called before poll_frame.

        :param exp_time: The exposure time.

        :return: None.
        """
        self.cam.start_seq(exp_time, num_frames)
    
    def poll_frame(self, timeout_ms: int=WAIT_FOREVER, oldestFrame: bool=True, 
                   copyData: bool=True) -> tuple[dict, float, int]:
        """
        Returns a single frame as a dictionary with optional meta data if available. 
        This method must be called after either stat_live or start_seq and before either abort or finish. 
        Pixel data can be accessed via the pixel_data key. Available meta data can be accessed via the meta_data key.

        If multiple ROIs are set, pixel data will be a list of region pixel data of length number of ROIs. 
        Meta data will also contain information for each ROI.

        Use set_param(constants.PARAM_METADATA_ENABLED, True) to enable meta data.

        :param timeout_ms: Duration to wait for new frames.
        :param oldestFrame: If True, the returned frame will the oldest frame and will be popped off the queue. 
                            If False, the returned frame will be the newest frame and will not be removed from the queue.
        :param copyData: Selects whether to return a copy of the numpy frame which points to a new buffer, or the original numpy frame which points to the
                         buffer used directly by PVCAM. Disabling this copy is not recommended for most situations. Refer to PyVCAM Wrapper.md for more details.

        :return: A dictionary with the frame containing available meta data and 2D np.array pixel data, frames per second and frame count.
        """
        return self.cam.poll_frame(timeout_ms, oldestFrame, copyData)
    
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
        :width: Num pixels in serial direction.
        :height: Num pixels in parallel direction.

        :return: None.
        """
        self.cam.set_roi(s1, p1, width, height)
    
    def shape(self, roi_index: int=0) -> tuple[int, int]:
        """
        Returns the reshape factor to be used when acquiring a ROI. This is equivalent to an acquired images shape.
        
        :param roi_index: Index of ROI

        :return:
        """
        return self.cam.shape(roi_index)
    
    def read_enum(self, param_id: int) -> dict[str, int]:
        """
        Returns all settings names paired with their values of a parameter.

        :param param_id: The parameter ID.

        :return: A dictionary containing strings mapped to values.
        """
        return self.cam.read_enum(param_id)
    
    def binning(self) -> tuple[int, int]:
        """
        Returns current serial and parallel binning values

        :return:
        """
        return self.cam.binning
    
    def set_binning(self, value: Union[tuple[int, int], int]) -> None:
        """
        Changes binning. A single value wills et a square binning.
        Binning cannot be changed directly on the camera; but is used for setting up 
        acquisitions and returning correctly shaped images returned from get_frame and get_live_frame.
        Binning settings for individual ROIs is not supported.

        :param value: Desired binning value as a tuple, or int for a square binning.

        :return: None.
        """
        self.cam.binning = value
    
    def chip_name(self) -> str:
        """
        Returns the camera sensor's name as a string.

        :return:
        """
        return self.cam.chip_name
    
    def post_trigger_delay(self) -> int:
        """
        Returns the last acquisition's post-trigger delay reported by the camera in ms.
        
        :return:
        """
        return self.cam.post_trigger_delay
    
    def pre_trigger_delay(self) -> int:
        """
        Returns the last acquisition's pre-trigger delay reported by the camera in ms.

        :return:
        """
        return self.cam.pre_trigger_delay
    
    def scan_line_time(self) -> int:
        """
        Returns the scan line time in ns.

        :return:
        """
        return self.cam.scan_line_time
    
    def sensor_size(self) -> tuple[int, int]:
        """
        Returns the sensor size of the current camera in a tuple 
        in the form (serial sensor size, parallel sensor size)

        :return:
        """
        return self.cam.sensor_size
    
    def speed_table_index(self) -> int:
        """
        Returns the current numerical index of the speed table of the camera.
        """
        return self.cam.speed_table_index
    
    def set_speed_table_index(self, value: int) -> None:
        """
        Changes the current numerical index of the speed table of the camera. 
        See the Port and Speed Choices section inside the PVCAM User Manual for 
        a detailed explanation about PVCAM speed tables.

        :param value: Desired speed table index.

        :return: None.
        """
        self.cam.speed_table_index = value
    
    def temp(self) -> int:
        """
        Returns the current temperature of a camera in Celsius.

        :return:
        """
        return self.cam.temp
    
    def temp_setpoint(self) -> int:
        """
        Returns the camera's temperature setpoint. The temperature setpoint is 
        the temperature that a camera will attempt to keep it's temperature (in Celsius) at.

        :return:
        """
        return self.cam.temp_setpoint
    
    def trigger_table(self, value: int) -> None:
        """
        Changes the camera's temperature setpoint.

        :return: None.
        """
        self.cam.temp_setpoint = value