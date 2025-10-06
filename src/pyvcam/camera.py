from copy import deepcopy
import functools
import os
import time
from typing import Dict, List, Optional, Tuple
import warnings

import numpy as np

from pyvcam import pvc
from pyvcam import constants as const


def deprecated(reason='This function is deprecated.'):
    def decorator(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            warnings.warn(f'Deprecated: {func.__name__} - {reason}',
                          category=DeprecationWarning,
                          stacklevel=2)
            return func(*args, **kwargs)
        return wrapper
    return decorator


class Camera:
    """Models a class currently connected to the system.

    Attributes:
        __name(str): String containing the name of the camera.
        __handle(int): The current camera's handle.
        __is_open(bool): True if camera is opened, False otherwise.

        __mode(int): The bit-wise OR between exposure mode and expose out mode.
        __exp_time(int): Integer representing the exposure time to be used for captures.
    """

    class ReversibleEnumDict(dict):
        # Helper class to populate enumerated parameter dictionaries and ease conversion
        # of keys and values. The param_id must support enum attribute.
        # This dictionary will accept both keys and values to the __getitem__ operator,
        # rather than just keys like regular dictionaries. If a value is provided,
        # the matching key is returned. The camera can be None only to initialize
        # the empty enum with name in Camera constructor.
        def __init__(self, name, camera=None, param_id=0):
            enum_dict = {}
            if camera is not None and param_id != 0:
                try:
                    enum_dict = camera.read_enum(param_id)
                except AttributeError:
                    pass

            super(Camera.ReversibleEnumDict, self).__init__(enum_dict)
            self.name = name
            self.__camera = camera

        def __getitem__(self, key_or_value):
            if self.__camera is None or not self.__camera.is_open:
                raise RuntimeError(f'Failed to access {self.name} enum, camera is not open')
            try:
                if isinstance(key_or_value, str):
                    return super(Camera.ReversibleEnumDict, self).__getitem__(key_or_value)
                return [key for key, item_value in self.items() if key_or_value == item_value][0]
            except KeyError as ex:
                raise ValueError(f"Invalid key '{key_or_value}' for {self.name}"
                                 f" - Available keys are: {list(self.keys())}") from ex
            except IndexError as ex:
                raise ValueError(f"Invalid value '{key_or_value}' for {self.name}"
                                 f" - Available values are: {list(self.values())}") from ex

    class RegionOfInterest:

        def __init__(self, s1, s2, sbin, p1, p2, pbin):
            if s1 < 0 or s2 < 0 or p1 < 0 or p2 < 0:
                raise ValueError('Coordinates must be >= 0')
            if s1 > s2:
                raise ValueError('Coordinates must be s1 <= s2')
            if p1 > p2:
                raise ValueError('Coordinates must be p1 <= p2')
            if sbin <= 0 or pbin <= 0:
                raise ValueError('Binning must be >= 1')
            # TODO: s1 & p1 could be also aligned to binning factors
            self.s1 = s1
            self.s2 = s2 - ((s2 - s1 + 1) % sbin)  # Clip partially binned pixels
            self.sbin = sbin
            self.p1 = p1
            self.p2 = p2 - ((p2 - p1 + 1) % pbin)  # Clip partially binned pixels
            self.pbin = pbin

        def __eq__(self, other):
            if not isinstance(other, Camera.RegionOfInterest):
                raise NotImplementedError

            # TODO: Add also binning factors to the comparison?
            return (
                self.s1 == other.s1 and
                self.s2 == other.s2 and
                self.p1 == other.p1 and
                self.p2 == other.p2)

        def check_overlap(self, other):
            return not (self.s2 < other.s1 or other.s2 < self.s1 or
                        self.p2 < other.p1 or other.p2 < self.p1)

        @property
        def shape(self):
            return (
                int((self.s2 - self.s1 + 1) / self.sbin),
                int((self.p2 - self.p1 + 1) / self.pbin))

    WAIT_FOREVER = -1

    def __init__(self, name):
        """NOTE: CALL Camera.detect_camera() to get a camera object."""

        self.__name: str = name
        self.__handle: int = -1
        self.__is_open: bool = False

        # Cached values update upon camera open
        self.__sensor_size: Tuple[int, int] = (0, 0)
        self.__has_bit_depth_host: bool = False
        self.__has_speed_name: bool = False
        self.__has_gain_name: bool = False

        # TODO: Define an enum for acquisition mode
        self.__acquisition_mode: Optional[str] = None  # 'Live', 'Sequence' or None

        # Exposure Settings
        self.__exp_mode: int = const.TIMED_MODE
        self.__exp_out_mode: int = const.EXPOSE_OUT_FIRST_ROW
        self.__mode: int = self.__exp_mode | self.__exp_out_mode
        self.__exp_time: int = 0

        # Regions of interest
        self.__default_roi: Camera.RegionOfInterest = \
            Camera.RegionOfInterest(s1=0, s2=0, sbin=1, p1=0, p2=0, pbin=1)
        self.__rois: List[Camera.RegionOfInterest] = []

        # Binning factors if the camera doesn't support arbitrary binning
        self.__limited_binnings: Optional[List[Tuple[int, int]]] = None

        # Enumeration parameters
        self.__readout_ports: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('readout_ports')
        self.__centroids_modes: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('centroids_modes')
        self.__clear_modes: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('clear_modes')
        self.__exp_modes: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('exp_modes')
        self.__exp_out_modes: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('exp_out_modes')
        self.__exp_resolutions: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('exp_resolutions')
        self.__fan_speeds: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('fan_speeds')
        self.__prog_scan_modes: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('prog_scan_modes')
        self.__prog_scan_dirs: Camera.ReversibleEnumDict = \
            Camera.ReversibleEnumDict('prog_scan_dirs')

        self.__port_speed_gain_table: Dict = {}
        self.__post_processing_table: Dict = {}

        self.__dtype = np.dtype('u2')  # uns16 by default

    def __repr__(self):
        return self.__name

    @staticmethod
    def get_available_camera_names():
        """Gets the name for each available camera.

        Returns:
           List of camera names, sorted by index.
        """

        ret = []

        total = pvc.get_cam_total()
        for index in range(total):
            ret.append(pvc.get_cam_name(index))

        return ret

    @classmethod
    def detect_camera(cls):
        """Detects and creates a new Camera object.

        Returns:
            A Camera object generator.
        """

        cam_count = 0
        total = pvc.get_cam_total()
        while cam_count < total:
            try:
                yield Camera(pvc.get_cam_name(cam_count))
                cam_count += 1
            except RuntimeError as ex:
                raise RuntimeError('Failed to create a detected camera.') from ex

    @classmethod
    def select_camera(cls, name):
        """Select camera by name and creates a new Camera object.

        Returns:
            A Camera object.
        """

        total = pvc.get_cam_total()
        for index in range(total):
            try:
                if name == pvc.get_cam_name(index):
                    return Camera(name)
            except RuntimeError as ex:
                raise RuntimeError('Failed to create a detected camera.') from ex

        raise RuntimeError('Failed to create a detected camera. Invalid name')

    def open(self):
        """Opens the camera.

        Side Effect(s):
            - changes self.__handle upon successful call to pvc module.
            - changes self.__is_open to True
            - changes self.__roi to sensor's full frame

        Returns:
            None
        """

        try:
            self.__handle = pvc.open_camera(self.__name)
            self.__is_open = True
        except RuntimeError as ex:
            raise RuntimeError('Failed to open camera.') from ex

        # If the camera is frame transfer capable, then set its p-mode to
        # frame transfer, otherwise set it to normal mode.
        try:
            self.get_param(const.PARAM_FRAME_CAPABLE)
            self.set_param(const.PARAM_PMODE, const.PMODE_FT)
        except AttributeError:
            self.set_param(const.PARAM_PMODE, const.PMODE_NORMAL)

        # Cache static values
        self.__sensor_size = (self.get_param(const.PARAM_SER_SIZE),
                              self.get_param(const.PARAM_PAR_SIZE))
        self.__has_bit_depth_host = self.check_param(const.PARAM_BIT_DEPTH_HOST)
        self.__has_speed_name = self.check_param(const.PARAM_SPDTAB_NAME)
        self.__has_gain_name = self.check_param(const.PARAM_GAIN_NAME)

        # Set ROI to full frame with no binning
        self.__default_roi = Camera.RegionOfInterest(
            s1=0, s2=self.__sensor_size[0] - 1, sbin=1,
            p1=0, p2=self.__sensor_size[1] - 1, pbin=1)
        self.reset_rois()

        # Cache extended binning factors if supported
        supports_bin_x = self.check_param(const.PARAM_BINNING_SER)
        supports_bin_y = self.check_param(const.PARAM_BINNING_PAR)
        if supports_bin_x and supports_bin_y:
            try:
                bin_xs = self.read_enum(const.PARAM_BINNING_SER).values()
                bin_ys = self.read_enum(const.PARAM_BINNING_PAR).values()
                self.__limited_binnings = list(zip(bin_xs, bin_ys))
            except AttributeError:
                self.__limited_binnings = None

        # Setup correct mode
        self.__exp_mode = self.get_param(const.PARAM_EXPOSURE_MODE)
        if self.check_param(const.PARAM_EXPOSE_OUT_MODE):
            self.__exp_out_mode = self.get_param(const.PARAM_EXPOSE_OUT_MODE)
        else:
            self.__exp_out_mode = 0
        self.__mode = self.__exp_mode | self.__exp_out_mode

        # Populate enumerated values
        self.__readout_ports = Camera.ReversibleEnumDict(
            'readout_ports', self, const.PARAM_READOUT_PORT)
        self.__centroids_modes = Camera.ReversibleEnumDict(
            'centroids_modes', self, const.PARAM_CENTROIDS_MODE)
        self.__clear_modes = Camera.ReversibleEnumDict(
            'clear_modes', self, const.PARAM_CLEAR_MODE)
        self.__exp_modes = Camera.ReversibleEnumDict(
            'exp_modes', self, const.PARAM_EXPOSURE_MODE)
        self.__exp_out_modes = Camera.ReversibleEnumDict(
            'exp_out_modes', self, const.PARAM_EXPOSE_OUT_MODE)
        self.__exp_resolutions = Camera.ReversibleEnumDict(
            'exp_resolutions', self, const.PARAM_EXP_RES)
        self.__fan_speeds = Camera.ReversibleEnumDict(
            'fan_speeds', self, const.PARAM_FAN_SPEED_SETPOINT)
        self.__prog_scan_modes = Camera.ReversibleEnumDict(
            'prog_scan_modes', self, const.PARAM_SCAN_MODE)
        self.__prog_scan_dirs = Camera.ReversibleEnumDict(
            'prog_scan_dirs', self, const.PARAM_SCAN_DIRECTION)

        # Learn ports, speeds and gains
        self.__port_speed_gain_table = {}
        for port_name, port_value in self.read_enum(const.PARAM_READOUT_PORT).items():
            self.readout_port = port_value

            self.__port_speed_gain_table[port_name] = {
                'port_value': port_value,
            }

            num_speeds = self.get_param(const.PARAM_SPDTAB_INDEX, const.ATTR_COUNT)
            for speed_index in range(num_speeds):
                self.speed = speed_index

                speed_name = self.speed_name

                gain_min = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_MIN)
                gain_max = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_MAX)
                gain_increment = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_INCREMENT)

                num_gains = int((gain_max - gain_min) / gain_increment + 1)
                gains = [(gain_min + i * gain_increment) for i in range(num_gains)]

                self.__port_speed_gain_table[port_name][speed_name] = {
                    'speed_index': speed_index,
                    'pixel_time': self.pix_time,
                    'bit_depth': self.bit_depth,
                    'gain_range': gains,
                }

                for gain_index in gains:
                    self.gain = gain_index

                    gain_name = self.gain_name

                    self.__port_speed_gain_table[port_name][speed_name][gain_name] = {
                        'gain_index': gain_index,
                    }

        # Reset speed table back to default
        self.readout_port = 0
        self.speed = 0
        self.gain = 1

        # Learn post-processing features
        self.__post_processing_table = {}
        try:
            feature_count = self.get_param(const.PARAM_PP_INDEX, const.ATTR_COUNT)
            for feature_index in range(feature_count):
                self.set_param(const.PARAM_PP_INDEX, feature_index)

                feature_id = self.get_param(const.PARAM_PP_FEAT_ID)
                feature_name = self.get_param(const.PARAM_PP_FEAT_NAME)
                self.__post_processing_table[feature_name] = {}

                param_count = self.get_param(const.PARAM_PP_PARAM_INDEX, const.ATTR_COUNT)
                for param_index in range(param_count):
                    self.set_param(const.PARAM_PP_PARAM_INDEX, param_index)

                    param_id = self.get_param(const.PARAM_PP_PARAM_ID)
                    param_name = self.get_param(const.PARAM_PP_PARAM_NAME)
                    param_min = self.get_param(const.PARAM_PP_PARAM, const.ATTR_MIN)
                    param_max = self.get_param(const.PARAM_PP_PARAM, const.ATTR_MAX)
                    self.__post_processing_table[feature_name][param_name] = {
                        'feature_index': feature_index,
                        'feature_id': feature_id,
                        'param_index': param_index,
                        'param_id': param_id,
                        'param_min': param_min,
                        'param_max': param_max,
                    }

        except AttributeError:
            pass

    def close(self):
        """Closes the camera.

        Side Effect(s):
            - changes self.__handle upon successful call to pvc module.
            - changes self.__is_open to False

        Returns:
            None
        """

        try:
            pvc.close_camera(self.__handle)

            self.__handle = -1
            self.__is_open = False

            # Cleanup internal state
            self.__sensor_size = (0, 0)
            self.__has_bit_depth_host = False
            self.__has_speed_name = False
            self.__has_gain_name = False
            self.__acquisition_mode = None
            self.__exp_mode = const.TIMED_MODE
            self.__exp_out_mode = const.EXPOSE_OUT_FIRST_ROW
            self.__mode = self.__exp_mode | self.__exp_out_mode
            self.__exp_time = 0
            self.__default_roi = Camera.RegionOfInterest(0, 0, 1, 0, 0, 1)
            self.__rois = []
            self.__limited_binnings = None
            self.__readout_ports = Camera.ReversibleEnumDict('readout_ports')
            self.__centroids_modes = Camera.ReversibleEnumDict('centroids_modes')
            self.__clear_modes = Camera.ReversibleEnumDict('clear_modes')
            self.__exp_modes = Camera.ReversibleEnumDict('exp_modes')
            self.__exp_out_modes = Camera.ReversibleEnumDict('exp_out_modes')
            self.__exp_resolutions = Camera.ReversibleEnumDict('exp_resolutions')
            self.__fan_speeds = Camera.ReversibleEnumDict('fan_speeds')
            self.__prog_scan_modes = Camera.ReversibleEnumDict('prog_scan_modes')
            self.__prog_scan_dirs = Camera.ReversibleEnumDict('prog_scan_dirs')
            self.__port_speed_gain_table = {}
            self.__post_processing_table = {}

        except RuntimeError as ex:
            raise RuntimeError('Failed to close camera.') from ex

    def check_frame_status(self):
        """Gets the frame transfer status.

        Will raise an exception if called prior to initiating acquisition.

        Parameter(s):
            None

        Returns:
            String representation of PL_IMAGE_STATUSES enum from pvcam.h.
            'READOUT_NOT_ACTIVE' - The system is @b idle, no data is expected.
                If any arrives, it will be discarded.
            'EXPOSURE_IN_PROGRESS' - The data collection routines are @b active.
                They are waiting for data to arrive, but none has arrived yet.
            'READOUT_IN_PROGRESS' - The data collection routines are @b active.
                The data has started to arrive.
            'READOUT_COMPLETE' - All frames available in sequence mode.
            'FRAME_AVAILABLE' - At least one frame is available in live mode.
            'READOUT_FAILED' - Something went wrong.
        """

        status = pvc.check_frame_status(self.__handle)

        # TODO: PVCAM currently returns FRAME_AVAILABLE/READOUT_COMPLETE after
        #       a sequence is finished. Until this behavior is resolved,
        #       force status to READOUT_NOT_ACTIVE while not acquiring.
        status = 'READOUT_NOT_ACTIVE' if (self.__acquisition_mode is None) else status
        return status

    def get_param(self, param_id, param_attr=const.ATTR_CURRENT):
        """Gets the value of a specified parameter and attribute.

        The type of the value returned depends on parameter type and attribute.

        Some attributes provide a value of the same type for all parameters:

        - ATTR_AVAIL: Always returns a `bool`.
                      It doesn't fail even for unavailable and unknown parameter ID.
        - ATTR_TYPE: Always returns an `int`.
                     It doesn't fail even for unavailable parameter.
                     An actual type for parameter attributes min., max., increment,
                     default and current.
        - ATTR_ACCESS: Always returns an `int`.
                       It doesn't fail even for unavailable parameter.
                       Defines whether the parameter is read-only, read-write, etc.
        - ATTR_LIVE: Always returns a `bool`.
                     If True, the parameter can be accessed also during acquisition.
        - ATTR_COUNT: Always return an `int`.
                      The meaning varies with parameter type. For example,
                      it defines number of enumeration items of TYPE_ENUM
                      parameter (use `read_enum` function to get all items),
                      number of characters in string for TYPE_CHAR_PTR parameters.
                      For numeric parameters it reports number of valid values
                      between min. and max. and given increment
                      (e.g. if min=0, max=10, inc=2, the count will be 6),
                      or zero if either all values for given types are supported
                      or the number of valid values doesn't fit `uns32` C type.

        Remaining attributes (ATTR_CURRENT, ATTR_MIN, ATTR_MAX, ATTR_INCREMENT,
        ATTR_DEFAULT) provide a value with type according to parameter type:

        - TYPE_INT*, TYPE_UNS*, TYPE_ENUM: Returns an `int`.
        - TYPE_FLT*: Returns a `float`.
        - TYPE_CHAR_PTR: Returns a `str`.
        - TYPE_RGN_TYPE: Returns a `dict` with keys `s1`, `s2`, `sbin`, `p1`, `p2`, `pbin`.
        - TYPE_SMART_STREAM_TYPE_PTR: Returns a `list` of `int` values.
                    The list can be empty, or in case of ATTR_MAX it contains
                    all zeroes where th number of zeroes means the max. number
                    of smart stream exposures.
                    **Warning**: It is known FW bug, the cameras actually
                    supports max-1 values only.

        Parameter(s):
            param_id (int): The parameter to get. Refer to constants.py for
                            defined constants for each parameter.
            param_attr (int): The desired attribute of the parameter to
                              identify. Refer to constants.py for defined
                              constants for each attribute.

        Returns:
            Value of specified parameter and attribute.
        """

        return pvc.get_param(self.__handle, param_id, param_attr)

    def set_param(self, param_id, value):
        """Sets a specified setting of a camera to a specified value.

        Note that pvc will raise a RuntimeError if the camera setting can not be
        applied. Pvc will also raise a ValueError if the supplied arguments are
        invalid for the specified parameter.

        Side Effect(s):
            - changes camera's internal setting.

        Parameters:
            param_id (int): An int that corresponds to a camera setting. Refer to
                            constants.py for valid parameter values.
            value (Varies): The value to set the camera setting to.
                            See the get_param documentation for value type.
        """

        pvc.set_param(self.__handle, param_id, value)

    def check_param(self, param_id):
        """Checks if a specified setting of a camera is available to read/modify.

        Side Effect(s):
            - None

        Parameters:
            param_id (int): An int that corresponds to a camera setting. Refer to
                            constants.py for valid parameter values.

        Returns:
            Boolean true if available, false if not
        """

        return pvc.check_param(self.__handle, param_id)

    def read_enum(self, param_id):
        """ Returns all settings names paired with their values of a parameter.

        Parameter:
            param_id (int):  The parameter ID.

        Returns:
            A dictionary containing strings mapped to values.
        """

        return pvc.read_enum(self.__handle, param_id)

    def reset_pp(self):
        """Resets the post-processing settings to default.

        Returns:
            None
        """

        pvc.reset_pp(self.__handle)

    def _update_mode(self):
        """Updates the mode of the camera.

        The mode is the bit-wise OR between exposure mode and expose out mode.
        It then sets up a small sequence so the exposure mode and expose out mode
        getters will read properly. This function should only be called internally
        whenever either exposure setting is changed.

        Side Effect(s):
            - Changes self.__mode
            - Sets up a small sequence so the camera will read out the exposure
              modes correctly with get_param.

        Returns:
            None
        """

        self.__mode = self.__exp_mode | self.__exp_out_mode
        pvc.set_exp_modes(self.__handle, self.__mode)

    def reset_rois(self):
        """Resets the ROI list to default, which is full frame.

        Returns:
            None
        """

        if not self.__is_open:
            raise RuntimeError('Camera is not open')

        self.__rois = [self.__default_roi]

    def set_roi(self, s1, p1, w, h):
        """Configures a ROI for the camera.

        The default ROI is the full frame. If the default is set or only a single
        ROI is supported, this function will over-write that ROI. Otherwise, this
        function will attempt to append this ROI to the list.

        Returns:
            None
        """

        if not self.__is_open:
            raise RuntimeError('Camera is not open')

        if w < 1 or h < 1:
            raise ValueError('Width and height must be >= 1')

        s2 = s1 + w - 1
        p2 = p1 + h - 1

        # Check if new ROI is in bounds
        in_bounds = (
            0 <= s1 < self.__sensor_size[0] and
            0 <= s2 < self.__sensor_size[0] and
            0 <= p1 < self.__sensor_size[1] and
            0 <= p2 < self.__sensor_size[1])
        if not in_bounds:
            raise ValueError('Could not add ROI. ROI extends beyond limits of sensor')

        # Check if current ROI list only contains the default
        using_default_roi = False
        if len(self.__rois) == 1:
            using_default_roi = self.__rois[0] == self.__default_roi

        # Get the total supported ROIs
        max_roi_count = self.get_param(const.PARAM_ROI_COUNT, const.ATTR_MAX)

        sbin = self.__rois[0].sbin
        pbin = self.__rois[0].pbin
        new_roi = Camera.RegionOfInterest(s1, s2, sbin, p1, p2, pbin)
        if max_roi_count == 1 or using_default_roi:
            self.__rois = [new_roi]
        elif len(self.__rois) < max_roi_count:
            # Check that new ROI doesn't overlap with existing ROIs
            for roi in self.__rois:
                if roi.check_overlap(new_roi):
                    raise ValueError('Could not add ROI. New ROI overlaps existing ROI')
            self.__rois.append(new_roi)
        else:
            raise ValueError(f'Could not add ROI. Camera only supports {max_roi_count} rois')

    # TODO: Remove this pylint suppression
    # Disabling CamelCase naming for now to not break existing scripts
    # pylint: disable=invalid-name
    def poll_frame(self, timeout_ms=WAIT_FOREVER, oldestFrame=True, copyData=True):
        """Calls the pvc.get_frame function with the current camera settings.

        Parameter:
            timeout_ms (int): Duration to wait for new frames.
            oldestFrame (bool):
                Selects whether to return the oldest or newest frame.
                Only the oldest frame will be popped off the underlying queue of frames.
            copyData (bool):
                Selects whether to return a copy of the numpy frame which points to
                a new buffer, or the original numpy frame which points to the buffer
                used directly by PVCAM. Disabling this copy is not recommended for most
                situations. Refer to PyVCAM Wrapper.md for more details.

        Returns:
            A dictionary with the frame containing available metadata and 2D np.array
            pixel data, frames per second and frame count.
        """

        frame, fps, frame_count = pvc.get_frame(
            self.__handle, self.__rois, self.__dtype.num, timeout_ms, oldestFrame)

        if copyData:
            # Shallow copy of the dict
            frame_tmp = frame.copy()
            # Deep copy the data
            frame_tmp['pixel_data'] = [np.copy(arr) for arr in frame['pixel_data']]
            if 'meta_data' in frame.keys():
                frame_tmp['meta_data'] = deepcopy(frame['meta_data'])

            frame = frame_tmp

        # If using a single ROI, remove list container
        if len(frame['pixel_data']) == 1:
            frame['pixel_data'] = frame['pixel_data'][0]

        return frame, fps, frame_count

    def get_frame(self, exp_time=None, timeout_ms=WAIT_FOREVER,
                  reset_frame_counter=False):
        """Calls the pvc.get_frame function with the current camera settings.

        Parameter:
            exp_time (int): The exposure time.
            timeout_ms (int): Duration to wait for new frames.
            reset_frame_counter (bool): Reset frame_count returned by poll_frame.
        Returns:
            A 2D np.array containing the pixel data from the captured frame.
        """

        self.start_seq(exp_time=exp_time, reset_frame_counter=reset_frame_counter)
        frame, _, _ = self.poll_frame(timeout_ms=timeout_ms)
        self.finish()

        return frame['pixel_data']

    def get_sequence(self, num_frames, exp_time=None, timeout_ms=WAIT_FOREVER,
                     interval=None, reset_frame_counter=False):
        """Calls the pvc.get_frame function with the current camera settings in
            rapid-succession for the specified number of frames.

        Parameter:
            num_frames (int): Number of frames to capture in the sequence
            exp_time (int): The exposure time
            timeout_ms (int): Duration to wait for new frames.
            interval (int): The time in milliseconds to wait between captures
            reset_frame_counter (bool): Reset frame_count returned by poll_frame.
        Returns:
            A 3D np.array containing the pixel data from the captured frames.
        """

        if len(self.__rois) > 1:
            raise ValueError('get_sequence does not support multi-roi captures')

        shape = self.__rois[0].shape
        stack = np.empty((num_frames, shape[1], shape[0]), dtype=self.__dtype)

        for i in range(num_frames):
            stack[i] = self.get_frame(exp_time=exp_time, timeout_ms=timeout_ms,
                                      reset_frame_counter=reset_frame_counter)
            reset_frame_counter = False

            if isinstance(interval, int) and i + 1 < num_frames:
                time.sleep(interval / 1000)

        return stack

    def get_vtm_sequence(self, time_list, exp_res, num_frames, timeout_ms=WAIT_FOREVER,
                         interval=None, reset_frame_counter=False):
        """Calls the pvc.get_frame function within a loop, setting vtm expTime
            between each capture.

        Parameter:
            time_list (list of ints): List of vtm timings
            exp_res (int): vtm exposure time resolution
                (0:milli, 1:micro, 2:seconds, use constants.EXP_RES_ONE_*SEC)
            num_frames (int): Number of frames to capture in the sequence
            timeout_ms (int): Duration to wait for new frames.
            interval (int): The time in milliseconds to wait between captures
            reset_frame_counter (bool): Reset frame_count returned by poll_frame.
        Returns:
            A 3D np.array containing the pixel data from the captured sequence.
        """

        if len(self.__rois) > 1:
            raise ValueError('get_vtm_sequence does not support multi-roi captures')

        shape = self.__rois[0].shape
        stack = np.empty((num_frames, shape[1], shape[0]), dtype=self.__dtype)

        old_res = self.exp_res
        self.exp_res = exp_res

        try:
            uses_vtm = self.exp_mode == const.VARIABLE_TIMED_MODE
            if uses_vtm:  # Native VTM

                if reset_frame_counter:
                    pvc.reset_frame_counter(self.__handle)

                # Use non-zero exposure time for VTM, the actual will be set later
                pvc.setup_seq(self.__handle, self.__rois, 1, self.__mode, 1)
                self.__acquisition_mode = 'Sequence'

                for i in range(num_frames):
                    exp_time = time_list[i % len(time_list)]

                    self.vtm_exp_time = exp_time
                    pvc.start_set_seq(self.__handle)

                    frame, _, _ = self.poll_frame(timeout_ms=timeout_ms)
                    stack[i] = frame['pixel_data']

                    if isinstance(interval, int) and i + 1 < num_frames:
                        time.sleep(interval / 1000)

                self.finish()

            else:  # Emulated VTM, very similar to get_sequence()

                for i in range(num_frames):
                    exp_time = time_list[i % len(time_list)]

                    stack[i] = self.get_frame(exp_time=exp_time,
                                              timeout_ms=timeout_ms,
                                              reset_frame_counter=reset_frame_counter)
                    reset_frame_counter = False

                    if isinstance(interval, int) and i + 1 < num_frames:
                        time.sleep(interval / 1000)

        finally:
            self.exp_res = old_res

        return stack

    def start_live(self, exp_time=None, buffer_frame_count=16,
                   stream_to_disk_path=None, reset_frame_counter=False):
        """Calls the pvc.start_live function to set up a circular buffer acquisition.

        Parameter:
            exp_time (int): The exposure time.
            buffer_frame_count (int): The number of frames in circ. buffer.
            stream_to_disk_path (str): None, or location where to save the data.
            reset_frame_counter (bool): Reset frame_count returned by poll_frame.
        Returns:
            None
        """

        if not isinstance(exp_time, int):
            exp_time = self.exp_time

        if isinstance(stream_to_disk_path, str):
            stream_to_disk_path_abs = os.path.abspath(stream_to_disk_path)
            directory, filename = os.path.split(stream_to_disk_path_abs)
            if os.path.exists(directory):
                try:
                    os.remove(filename)
                except OSError:
                    pass
            else:
                raise ValueError(f'Invalid directory for stream to disk: {directory}')

        if reset_frame_counter:
            pvc.reset_frame_counter(self.__handle)

        self.__acquisition_mode = 'Live'
        pvc.start_live(self.__handle, self.__rois, exp_time, self.__mode,
                       buffer_frame_count, stream_to_disk_path)

    def start_seq(self, exp_time=None, num_frames=1, reset_frame_counter=False):
        """Calls the pvc.start_seq function to set up a non-circular buffer acquisition.

        Parameter:
            exp_time (int): The exposure time.
            num_frames (int): The number of frames in a sequence (max. 65535).
            reset_frame_counter (bool): Reset frame_count returned by poll_frame.
        Returns:
            None
        """

        if not isinstance(exp_time, int):
            exp_time = self.exp_time

        if reset_frame_counter:
            pvc.reset_frame_counter(self.__handle)

        self.__acquisition_mode = 'Sequence'
        pvc.start_seq(self.__handle, self.__rois, exp_time, self.__mode, num_frames)

    def finish(self):
        """Ends a previously started live or sequence acquisition.

        Parameter:
            None
        Returns:
            None
        """

        # In recent PVCAM pl_exp_abort, pl_exp_stop_cont and pl_exp_finish_seq
        # do exactly the same thing - abort an ongoing acquisition.
        # The only difference is that pl_exp_finish_seq doesn't take 'cam_state'
        # argument that has no meaning anymore and PyVCAM doesn't expose it.
        # Legacy PVCAM versions used pl_exp_finish_seq to finish frame data
        # post-processing if there were some PoP/Splice plugins installed.
        # We keep finish_seq a part of module for now.
        if self.__acquisition_mode == 'Live':
            pvc.abort(self.__handle)
        elif self.__acquisition_mode == 'Sequence':
            pvc.finish_seq(self.__handle)

        self.__acquisition_mode = None

    @deprecated("Use 'finish' function instead")
    def abort(self):
        return self.finish()

    def sw_trigger(self):
        """Performs an SW trigger. This trigger behaves analogously to a HW external trigger.
            Will throw an exception if trigger fails.

        Parameter:
            None
        Returns:
            None
        """

        pvc.sw_trigger(self.__handle)

    def set_post_processing_param(self, feature_name, param_name, value):
        """Sets the value of a post-processing parameter.

        Parameter:
            Feature name and parameter name as specified in post_processing_table
        Returns:
            None
        """

        if feature_name in self.__post_processing_table:
            if param_name in self.__post_processing_table[feature_name]:
                pp_param = self.__post_processing_table[feature_name][param_name]

                if pp_param['param_min'] <= value <= pp_param['param_max']:
                    self.set_param(const.PARAM_PP_INDEX, pp_param['feature_index'])
                    self.set_param(const.PARAM_PP_PARAM_INDEX, pp_param['param_index'])
                    self.set_param(const.PARAM_PP_PARAM, value)

                    self._set_dtype()
                    return

                raise AttributeError(
                    f"Could not set post processing param. Value {value} out of range "
                    f"({pp_param['param_min']}, {pp_param['param_max']})")
            raise AttributeError('Could not set post processing param. param_name not found')
        raise AttributeError('Could not set post processing param. feature_name not found')

    def get_post_processing_param(self, feature_name, param_name):
        """Gets the current value of a post-processing parameter.

        Parameter:
            Feature name and parameter name as specified in post_processing_table
        Returns:
            Value of specified post-processing parameter
        """

        if feature_name in self.__post_processing_table:
            if param_name in self.__post_processing_table[feature_name]:
                pp_param = self.__post_processing_table[feature_name][param_name]

                self.set_param(const.PARAM_PP_INDEX, pp_param['feature_index'])
                self.set_param(const.PARAM_PP_PARAM_INDEX, pp_param['param_index'])

                return self.get_param(const.PARAM_PP_PARAM)

            raise AttributeError('Could not set post processing param. param_name not found')
        raise AttributeError('Could not set post processing param. feature_name not found')

    def _set_dtype(self):
        if self.__has_bit_depth_host:
            bit_depth = self.get_param(const.PARAM_BIT_DEPTH_HOST)
        else:
            bit_depth = self.get_param(const.PARAM_BIT_DEPTH)
        bytes_per_pixel = int(np.ceil(bit_depth / 8))
        dtype_kind = f'u{bytes_per_pixel}'
        self.__dtype = np.dtype(dtype_kind)

    # # # Getters/Setters below # # #

    @property
    def handle(self):
        return self.__handle

    @property
    def is_open(self):
        return self.__is_open

    @property
    def name(self):
        return self.__name

    @property
    def post_processing_table(self):
        return self.__post_processing_table

    @property
    def port_speed_gain_table(self):
        return self.__port_speed_gain_table

    @property
    def readout_ports(self):
        return self.__readout_ports

    @property
    def centroids_modes(self):
        return self.__centroids_modes

    @property
    def clear_modes(self):
        return self.__clear_modes

    @property
    def exp_modes(self):
        return self.__exp_modes

    @property
    def exp_out_modes(self):
        return self.__exp_out_modes

    @property
    def exp_resolutions(self):
        return self.__exp_resolutions

    @property
    def fan_speeds(self):
        return self.__fan_speeds

    @property
    def prog_scan_modes(self):
        return self.__prog_scan_modes

    @property
    def prog_scan_dirs(self):
        return self.__prog_scan_dirs

    @property
    def driver_version(self):
        dd_ver = self.get_param(const.PARAM_DD_VERSION)
        # The device driver version is returned as a highly formatted 16-bit
        # integer where the first 8 bits are the major version, bits 9-12 are
        # the minor version, and bits 13-16 are the build number. Uses of masks
        # and bit shifts are required to extract the full version number.
        return f'{(dd_ver >> 8) & 0xFF}.'\
               f'{(dd_ver >> 4) & 0x0F}.'\
               f'{(dd_ver >> 0) & 0x0F}'

    @property
    def cam_fw(self):
        return pvc.get_cam_fw_version(self.__handle)

    @property
    def chip_name(self):
        return self.get_param(const.PARAM_CHIP_NAME)

    @property
    def sensor_size(self):
        if not self.__is_open:
            raise RuntimeError('Camera is not open')
        return self.__sensor_size

    @property
    def serial_no(self):
        # HACK: cytocam fix for messed up serial numbers
        try:
            serial_no = self.get_param(const.PARAM_HEAD_SER_NUM_ALPHA)
            return serial_no
        except AttributeError:
            return 'N/A'

    @property
    def bit_depth(self):
        return self.get_param(const.PARAM_BIT_DEPTH)

    @property
    def bit_depth_host(self):
        if self.__has_bit_depth_host:
            return self.get_param(const.PARAM_BIT_DEPTH_HOST)
        return self.get_param(const.PARAM_BIT_DEPTH)

    @property
    def pix_time(self):
        return self.get_param(const.PARAM_PIX_TIME)

    @property
    def readout_port(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_READOUT_PORT)

    @readout_port.setter
    def readout_port(self, key_or_value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__readout_ports[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__readout_ports[value]

        self.set_param(const.PARAM_READOUT_PORT, value)

        self._set_dtype()

    @property
    def speed(self):
        return self.get_param(const.PARAM_SPDTAB_INDEX)

    @speed.setter
    def speed(self, value):
        num_entries = self.get_param(const.PARAM_SPDTAB_INDEX,
                                     const.ATTR_COUNT)
        if value >= num_entries:
            raise ValueError(f'{self} only supports {num_entries} speed entries')
        self.set_param(const.PARAM_SPDTAB_INDEX, value)

        self._set_dtype()

    @property
    @deprecated("Use 'speed' property instead")
    def speed_table_index(self):
        return self.speed

    @speed_table_index.setter
    @deprecated("Use 'speed' property instead")
    def speed_table_index(self, value):
        self.speed = value

    @property
    def speed_name(self):
        if self.__has_speed_name:
            return self.get_param(const.PARAM_SPDTAB_NAME)
        return f'Speed_{self.speed}'

    @property
    def trigger_table(self):
        # Returns a dictionary containing information about the last capture.
        # Note some features are camera specific.

        # Default resolution is const.EXP_RES_ONE_MILLISEC
        # even if the parameter isn't available
        exp_unit = ' ms'
        try:
            exp_res = self.exp_res
            if exp_res == const.EXP_RES_ONE_SEC:
                exp_unit = ' s'
            elif exp_res == const.EXP_RES_ONE_MICROSEC:
                exp_unit = ' μs'
        except AttributeError:
            pass
        try:
            exp = str(self.last_exp_time) + exp_unit
        except AttributeError:
            exp = 'N/A'

        try:
            read = str(self.readout_time) + ' μs'
        except AttributeError:
            read = 'N/A'

        # If the camera has clear time, then it has pre- and post-trigger delays
        try:
            clear = str(self.clear_time) + ' ns'
            pre = str(self.pre_trigger_delay) + ' ns'
            post = str(self.post_trigger_delay) + ' ns'
        except AttributeError:
            clear = 'N/A'
            pre = 'N/A'
            post = 'N/A'

        return {'Exposure Time': exp,
                'Readout Time': read,
                'Clear Time': clear,
                'Pre-trigger Delay': pre,
                'Post-trigger Delay': post}

    @property
    def adc_offset(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_ADC_OFFSET)

    @property
    def gain(self):
        return self.get_param(const.PARAM_GAIN_INDEX)

    @gain.setter
    def gain(self, value):
        min_gain = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_MIN)
        max_gain = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_MAX)
        if not min_gain <= value <= max_gain:
            raise ValueError(f'Invalid value: {value} - {self} only supports '
                             f'gain indices from {min_gain} to {max_gain}.')
        self.set_param(const.PARAM_GAIN_INDEX, value)

        self._set_dtype()

    @property
    def gain_name(self):
        if self.__has_gain_name:
            return self.get_param(const.PARAM_GAIN_NAME)
        return f'Gain_{self.gain}'

    @property
    def binnings(self):
        if not self.__is_open:
            raise RuntimeError('Camera is not open')
        return self.__limited_binnings

    @property
    def binning(self):
        if not self.__is_open:
            raise RuntimeError('Camera is not open')
        return self.__rois[0].sbin, self.__rois[0].pbin

    @binning.setter
    def binning(self, value):
        if not self.__is_open:
            raise RuntimeError('Camera is not open')

        if isinstance(value, tuple):
            bin_x, bin_y = value
        else:
            bin_x = bin_y = value

        if bin_x <= 0 or bin_y <= 0:
            raise ValueError('Binning must be >= 1')

        # Validate the combination if the binning is not arbitrary
        if self.__limited_binnings is not None:
            if (bin_x, bin_y) not in self.__limited_binnings:
                raise ValueError(f'{self} only supports {self.__limited_binnings} binnings')

        # Update all ROIs to be ready for next acq. setup
        for roi in self.__rois:
            roi.sbin = bin_x
            roi.pbin = bin_y

    @property
    @deprecated("Use 'binning' property instead")
    def bin_x(self):
        return self.binning[0]

    @bin_x.setter
    @deprecated("Use 'binning' property instead")
    def bin_x(self, value):
        self.binning = (value, self.__rois[0].pbin)

    @property
    @deprecated("Use 'binning' property instead")
    def bin_y(self):
        return self.binning[1]

    @bin_y.setter
    @deprecated("Use 'binning' property instead")
    def bin_y(self, value):
        self.binning = (self.__rois[0].sbin, value)

    @property
    def rois(self):
        if not self.__is_open:
            raise RuntimeError('Camera is not open')
        return self.__rois

    def shape(self, roi_index=0):
        if not self.__is_open:
            raise RuntimeError('Camera is not open')
        return self.__rois[roi_index].shape

    @property
    def live_roi(self):
        return self.get_param(const.PARAM_ROI)

    @live_roi.setter
    def live_roi(self, value):
        self.set_param(const.PARAM_ROI, value)

    @property
    def last_exp_time(self):
        return self.get_param(const.PARAM_EXPOSURE_TIME)

    @property
    def exp_res(self):
        return self.get_param(const.PARAM_EXP_RES)

    @exp_res.setter
    def exp_res(self, key_or_value):
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__exp_resolutions[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__exp_resolutions[value]

        self.set_param(const.PARAM_EXP_RES, value)

    @property
    def exp_res_index(self):
        return self.get_param(const.PARAM_EXP_RES_INDEX)

    @property
    def exp_time(self):
        if not self.__is_open:
            raise RuntimeError('Camera is not open')
        return self.__exp_time

    @exp_time.setter
    def exp_time(self, value):
        min_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MIN)
        max_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MAX)
        if not min_exp_time <= value <= max_exp_time:
            raise ValueError(f'Invalid value: {value} - {self} only supports '
                             f'exposure times between {min_exp_time} and {max_exp_time}')
        self.__exp_time = value

    @property
    def exp_mode(self):
        return self.get_param(const.PARAM_EXPOSURE_MODE)

    @exp_mode.setter
    def exp_mode(self, key_or_value):
        # Will raise ValueError if provided with an unrecognized key.
        self.__exp_mode = self.__exp_modes[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__exp_modes[self.__exp_mode]

        self._update_mode()

    @property
    def exp_out_mode(self):
        return self.get_param(const.PARAM_EXPOSE_OUT_MODE)

    @exp_out_mode.setter
    def exp_out_mode(self, key_or_value):
        # Will raise ValueError if provided with an unrecognized key.
        self.__exp_out_mode = self.__exp_out_modes[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__exp_out_modes[self.__exp_out_mode]

        self._update_mode()

    @property
    def vtm_exp_time(self):
        return self.get_param(const.PARAM_EXP_TIME)

    @vtm_exp_time.setter
    def vtm_exp_time(self, value):
        min_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MIN)
        max_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MAX)
        max_exp_time = min(max_exp_time, 65535)  # uns16 limit
        if not min_exp_time <= value <= max_exp_time:
            raise ValueError(f'Invalid value: {value} - {self} only supports '
                             f'exposure times between {min_exp_time} and {max_exp_time}')
        self.set_param(const.PARAM_EXP_TIME, value)

    @property
    def clear_mode(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_CLEAR_MODE)

    @clear_mode.setter
    def clear_mode(self, key_or_value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__clear_modes[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__clear_modes[value]

        self.set_param(const.PARAM_CLEAR_MODE, value)

    @property
    def fan_speed(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_FAN_SPEED_SETPOINT)

    @fan_speed.setter
    def fan_speed(self, key_or_value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__fan_speeds[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__fan_speeds[value]

        self.set_param(const.PARAM_FAN_SPEED_SETPOINT, value)

    @property
    def temp(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_TEMP) / 100.0

    @property
    def temp_setpoint(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_TEMP_SETPOINT) / 100.0

    @temp_setpoint.setter
    def temp_setpoint(self, value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        temp = int(value * 100.0)
        min_temp = self.get_param(const.PARAM_TEMP_SETPOINT, const.ATTR_MIN)
        max_temp = self.get_param(const.PARAM_TEMP_SETPOINT, const.ATTR_MAX)
        if not min_temp <= temp <= max_temp:
            raise ValueError(f'Invalid temp {value} : Valid temps are in the range '
                             f'{min_temp / 100.0} - {max_temp / 100.0}.')
        self.set_param(const.PARAM_TEMP_SETPOINT, temp)

    @property
    def readout_time(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_READOUT_TIME)

    @property
    def clear_time(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_CLEARING_TIME)

    @property
    def pre_trigger_delay(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_PRE_TRIGGER_DELAY)

    @property
    def post_trigger_delay(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_POST_TRIGGER_DELAY)

    @property
    def centroids_mode(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_CENTROIDS_MODE)

    @centroids_mode.setter
    def centroids_mode(self, key_or_value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__centroids_modes[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__centroids_modes[value]

        self.set_param(const.PARAM_CENTROIDS_MODE, value)

    @property
    def prog_scan_mode(self):
        # Camera specific setting: Will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_SCAN_MODE)

    @prog_scan_mode.setter
    def prog_scan_mode(self, key_or_value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__prog_scan_modes[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__prog_scan_modes[value]

        self.set_param(const.PARAM_SCAN_MODE, value)

    @property
    def prog_scan_dir(self):
        # Camera specific setting: Will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_SCAN_DIRECTION)

    @prog_scan_dir.setter
    def prog_scan_dir(self, key_or_value):
        # Camera specific setting. Will raise AttributeError if called with a
        # camera that does not support this setting.
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__prog_scan_dirs[key_or_value] if isinstance(key_or_value, str) \
            else key_or_value
        # Verify value is in range by attempting to look up the key
        _ = self.__prog_scan_dirs[value]

        self.set_param(const.PARAM_SCAN_DIRECTION, value)

    @property
    def prog_scan_dir_reset(self):
        return self.get_param(const.PARAM_SCAN_DIRECTION_RESET)

    @prog_scan_dir_reset.setter
    def prog_scan_dir_reset(self, value):
        self.set_param(const.PARAM_SCAN_DIRECTION_RESET, value)

    @property
    def prog_scan_line_delay(self):
        return self.get_param(const.PARAM_SCAN_LINE_DELAY)

    @prog_scan_line_delay.setter
    def prog_scan_line_delay(self, value):
        self.set_param(const.PARAM_SCAN_LINE_DELAY, value)

    @property
    def prog_scan_line_time(self):
        return self.get_param(const.PARAM_SCAN_LINE_TIME)

    @property
    @deprecated("Use 'prog_scan_line_time' property instead")
    def scan_line_time(self):
        return self.prog_scan_line_time

    @property
    def prog_scan_width(self):
        return self.get_param(const.PARAM_SCAN_WIDTH)

    @prog_scan_width.setter
    def prog_scan_width(self, value):
        self.set_param(const.PARAM_SCAN_WIDTH, value)

    @property
    def metadata_enabled(self):
        return self.get_param(const.PARAM_METADATA_ENABLED)

    @metadata_enabled.setter
    def metadata_enabled(self, value):
        self.set_param(const.PARAM_METADATA_ENABLED, value)

    @property
    @deprecated("Use 'metadata_enabled' property instead")
    def meta_data_enabled(self):
        return self.metadata_enabled

    @meta_data_enabled.setter
    @deprecated("Use 'metadata_enabled' property instead")
    def meta_data_enabled(self, value):
        self.metadata_enabled = value

    @property
    def smart_stream_mode_enabled(self):
        return self.get_param(const.PARAM_SMART_STREAM_MODE_ENABLED)

    @smart_stream_mode_enabled.setter
    def smart_stream_mode_enabled(self, value):
        self.set_param(const.PARAM_SMART_STREAM_MODE_ENABLED, value)

    @property
    def smart_stream_mode(self):
        return self.get_param(const.PARAM_SMART_STREAM_MODE)

    @smart_stream_mode.setter
    def smart_stream_mode(self, value):
        self.set_param(const.PARAM_SMART_STREAM_MODE, value)

    @property
    def smart_stream_exp_params(self):
        return self.get_param(const.PARAM_SMART_STREAM_EXP_PARAMS)

    @smart_stream_exp_params.setter
    def smart_stream_exp_params(self, value):
        self.set_param(const.PARAM_SMART_STREAM_EXP_PARAMS, value)
