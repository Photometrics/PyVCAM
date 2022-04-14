from pyvcam import pvc
from pyvcam import constants as const

from copy import deepcopy
import os
import time
import numpy as np


class Camera:
    """Models a class currently connected to the system.

    Attributes:
        __name(str): String containing the name of the camera.
        __handle(int): The current camera's handle.
        __is_open(bool): True if camera is opened, False otherwise.

        __exposure_bytes(int): How large the buffer for live imaging needs to be.

        __mode(int): The bit-wise or between exposure mode and expose out mode.
        __exp_time(int): Integer representing the exposure time to be used for captures.
    """

    class ReversibleEnumDict(dict):
        # Helper class to populate enumerated parameter dictionaries and ease conversion of keys and values.
        # The param_id must support enum attribute
        # This dictionary will accept both keys and values to the __getitem__ operator, rather than just keys like
        # regular dictionaries. If a value is provided, the matching key is returned.
        def __init__(self, name, camera_instance, param_id):
            try:
                enumDict = camera_instance.read_enum(param_id)
            except AttributeError:
                enumDict = {}

            super(Camera.ReversibleEnumDict, self).__init__(enumDict)
            self.name = name

        def __getitem__(self, keyOrValue):
            try:
                if isinstance(keyOrValue, str):
                    return super(Camera.ReversibleEnumDict, self).__getitem__(keyOrValue)
                else:
                    return [key for key, item_value in self.items() if keyOrValue == item_value][0]
            except KeyError:
                raise ValueError('Invalid key: {0} for {1} - Available keys are: {2}'.format(keyOrValue, self.name, list(self.keys())))
            except IndexError:
                raise ValueError('Invalid value: {0} for {1} - Available values are: {2}'.format(keyOrValue, self.name, list(self.values())))

    class RegionOfInterest:

        def __init__(self, s1, s2, sbin, p1, p2, pbin):
            self.s1 = s1
            self.s2 = s2 - ((s2 - s1 + 1) % sbin)  # Clip partially binned pixels
            self.sbin = sbin
            self.p1 = p1
            self.p2 = p2 - ((p2 - p1 + 1) % pbin)  # Clip partially binned pixels
            self.pbin = pbin

        def __eq__(self, other):
            if not isinstance(other, Camera.RegionOfInterest):
                raise NotImplementedError

            return self.s1 == other.s1 and self.s2 == other.s2 and self.p1 == other.p1 and self.p2 == other.p2

        def checkOverlap(self, other):
            return (self.s1 <= other.s1 <= self.s1 and self.p1 <= other.p1 <= self.p1) or \
                   (self.s2 <= other.s2 <= self.s2 and self.p2 <= other.p2 <= self.p2)

        @property
        def shape(self):
            return int((self.s2 - self.s1 + 1) / self.sbin), int((self.p2 - self.p1 + 1) / self.pbin)

    WAIT_FOREVER = -1

    def __init__(self, name):
        """NOTE: CALL Camera.detect_camera() to get a camera object."""
        self.__name = name
        self.__handle = -1
        self.__is_open = False
        self.__acquisition_mode = None

        # Memory for live circular buffer
        self.__exposure_bytes = None

        # Exposure Settings
        self.__mode = None
        self.__exp_time = 0

        # Regions of interest
        self.__defaultRoi = None  # The default RegionOfInterest object
        self.__rois = []  # A list of RegionOfInterest objects

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
            except RuntimeError:
                raise RuntimeError('Failed to create a detected camera.')

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
            except RuntimeError:
                raise RuntimeError('Failed to create a detected camera.')

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
        except:
            raise RuntimeError('Failed to open camera.')

        # If the camera is frame transfer capable, then set its p-mode to
        # frame transfer, otherwise set it to normal mode.
        try:
            self.get_param(const.PARAM_FRAME_CAPABLE, const.ATTR_CURRENT)
            self.set_param(const.PARAM_PMODE, const.PMODE_FT)
        except AttributeError:
            self.set_param(const.PARAM_PMODE, const.PMODE_NORMAL)

        # Set ROI to full frame with no binning
        self.__defaultRoi = Camera.RegionOfInterest(0, self.sensor_size[0] - 1, 1, 0, self.sensor_size[1] - 1, 1)
        self.reset_rois()

        # Setup correct mode
        self.__exp_mode = self.get_param(const.PARAM_EXPOSURE_MODE)
        if self.check_param(const.PARAM_EXPOSE_OUT_MODE):
            self.__exp_out_mode = self.get_param(const.PARAM_EXPOSE_OUT_MODE)
        else:
            self.__exp_out_mode = 0

        self.__mode = self.__exp_mode | self.__exp_out_mode

        # Populate enumerated values
        self.__centroids_modes = Camera.ReversibleEnumDict('centroids_modes', self, const.PARAM_CENTROIDS_MODE)
        self.__clear_modes = Camera.ReversibleEnumDict('clear_modes', self, const.PARAM_CLEAR_MODE)
        self.__exp_modes = Camera.ReversibleEnumDict('exp_modes', self, const.PARAM_EXPOSURE_MODE)
        self.__exp_out_modes = Camera.ReversibleEnumDict('exp_out_modes', self, const.PARAM_EXPOSE_OUT_MODE)
        self.__exp_resolutions = Camera.ReversibleEnumDict('exp_resolutions', self, const.PARAM_EXP_RES)
        self.__prog_scan_modes = Camera.ReversibleEnumDict('prog_scan_modes', self, const.PARAM_SCAN_MODE)
        self.__prog_scan_dirs = Camera.ReversibleEnumDict('prog_scan_dirs', self, const.PARAM_SCAN_DIRECTION)

        # Learn ports, speeds and gains
        self.__port_speed_gain_table = {}
        for port_name, port_value in self.read_enum(const.PARAM_READOUT_PORT).items():
            self.__port_speed_gain_table[port_name] = {'port_value': port_value}
            self.readout_port = port_value
            num_speeds = self.get_param(const.PARAM_SPDTAB_INDEX, const.ATTR_COUNT)
            for speed_index in range(num_speeds):
                speed_name = 'Speed_' + str(speed_index)
                self.speed_table_index = speed_index

                gain_min = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_MIN)
                gain_max = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_MAX)
                gain_increment = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_INCREMENT)

                numGains = int((gain_max - gain_min) / gain_increment + 1)
                gains = [(gain_min + i * gain_increment) for i in range(numGains)]

                self.__port_speed_gain_table[port_name].update({speed_name: {'speed_index': speed_index, 'pixel_time': self.pix_time, 'bit_depth': self.bit_depth, 'gain_range': gains}})

                for gain_index in gains:
                    self.gain = gain_index
                    try:
                        gain_name = self.get_param(const.PARAM_GAIN_NAME, const.ATTR_CURRENT)
                    except:
                        gain_name = 'Gain_' + str(gain_index)

                    self.__port_speed_gain_table[port_name][speed_name][gain_name] = {'gain_index': gain_index}

        # Reset speed table back to default
        self.readout_port = 0
        self.speed_table_index = 0

        # Learn post processing features
        self.__post_processing_table = {}
        try:
            featureCount = self.get_param(const.PARAM_PP_INDEX, const.ATTR_COUNT)
            for featureIndex in range(featureCount):

                self.set_param(const.PARAM_PP_INDEX, featureIndex)
                featureId = self.get_param(const.PARAM_PP_FEAT_ID)
                featureName = self.get_param(const.PARAM_PP_FEAT_NAME)
                self.__post_processing_table[featureName] = {}

                paramCount = self.get_param(const.PARAM_PP_PARAM_INDEX, const.ATTR_COUNT)
                for paramIndex in range(paramCount):
                    self.set_param(const.PARAM_PP_PARAM_INDEX, paramIndex)
                    paramId = self.get_param(const.PARAM_PP_PARAM_ID)
                    paramName = self.get_param(const.PARAM_PP_PARAM_NAME)
                    paramMin = self.get_param(const.PARAM_PP_PARAM, const.ATTR_MIN)
                    paramMax = self.get_param(const.PARAM_PP_PARAM, const.ATTR_MAX)
                    self.__post_processing_table[featureName][paramName] = {'feature_index': featureIndex, 'feature_id': featureId, 'param_index': paramIndex, 'param_id': paramId, 'param_min': paramMin, 'param_max': paramMax}

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
        except:
            raise RuntimeError('Failed to close camera.')

    def check_frame_status(self):
        """Gets the frame transfer status. Will raise an exception if called prior to initiating acquisition

        Parameter(s):
            None

        Returns:
            String representation of PL_IMAGE_STATUSES enum from pvcam.h
            'READOUT_NOT_ACTIVE' - The system is @b idle, no data is expected. If any arrives, it will be discarded.
            'EXPOSURE_IN_PROGRESS' - The data collection routines are @b active. They are waiting for data to arrive, but none has arrived yet.
            'READOUT_IN_PROGRESS' - The data collection routines are @b active. The data has started to arrive.
            'READOUT_COMPLETE' - All frames available in sequnece mode.
            'FRAME_AVAILABLE' - At least one frame is available in live mode
            'READOUT_FAILED' - Something went wrong.
        """

        status = pvc.check_frame_status(self.__handle)

        # TODO:  pvcam currently returns FRAME_AVAILABLE/READOUT_COMPLETE after a sequence is finished. Until this behavior is resolved,
        #        force status to READOUT_NOT_ACTIVE while not acquiring
        status = 'READOUT_NOT_ACTIVE' if (self.__acquisition_mode is None) else status
        return status

    def get_param(self, param_id, param_attr=const.ATTR_CURRENT):
        """Gets the current value of a specified parameter.

        Parameter(s):
            param_id (int): The parameter to get. Refer to constants.py for
                            defined constants for each parameter.
            param_attr (int): The desired attribute of the parameter to
                              identify. Refer to constants.py for defined
                              constants for each attribute.

        Returns:
            Value of specified parameter.
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
        """
        pvc.set_param(self.__handle, param_id, value)

    def check_param(self, param_id):
        """Checks if a specified setting of a camera is available to read/ modify.

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
        try:
            pvc.reset_pp(self.__handle)
        except:
            raise RuntimeError('Failed to reset post-processing settings.')

    def _update_mode(self):
        """Updates the mode of the camera, which is the bit-wise or between
           exposure mode and expose out mode. It then sets up a small sequence
           so the exposure mode and expose out mode getters will read properly.
           This function should only be called internally whenever either exposure
           setting is changed.

        Side Effect(s):
            - Changes self.__mode
            - Sets up a small sequence so the camera will readout the exposure
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
        self.__rois = [self.__defaultRoi]

    def set_roi(self, s1, p1, w, h):
        """Configures a ROI for the camera. The default ROI is the full frame. If the default is
           set or only a single ROI is supported, this function will over-write that ROI. Otherwise,
           this function will attempt to append this ROI to the list.

        Returns:
            None
        """

        if w < 1 or h < 1:
            raise ValueError('Width and height must be >= 1')

        s2 = s1 + w - 1
        p2 = p1 + h - 1

        # Check if new ROI is in bounds
        inBounds = 0 <= s1 <= self.sensor_size[0] and \
            0 <= s2 <= self.sensor_size[0] and \
            0 <= p1 <= self.sensor_size[1] and \
            0 <= p2 <= self.sensor_size[1]

        if not inBounds:
            raise ValueError('Could not add ROI. ROI extends beyond limits of frame')

        # Get the total supported ROIs
        roiCount = self.get_param(const.PARAM_ROI_COUNT, const.ATTR_MAX)

        # Check if current ROI list only contains the default
        usingDefaultRoi = False
        if len(self.__rois) == 1:
            usingDefaultRoi = self.__rois[0] == self.__defaultRoi

        newRoi = Camera.RegionOfInterest(s1, s2, self.bin_x, p1, p2, self.bin_y)
        if roiCount == 1 or usingDefaultRoi:
            self.__rois = [newRoi]

        elif len(self.__rois) < roiCount:

            # Check that new ROI doesn't overlap with existing ROIs
            for roi in self.__rois:
                if roi.checkOverlap(newRoi):
                    raise ValueError('Could not add ROI. New ROI overlaps existing ROI')

            self.__rois.append(newRoi)
        else:
            raise ValueError('Could not add ROI. Camera only supports {} rois'.format(roiCount))

    def poll_frame(self, timeout_ms=WAIT_FOREVER, oldestFrame=True, copyData=True):
        """Calls the pvc.get_frame function with the current camera settings.

        Parameter:
            oldestFrame (bool): Selects whether to return the oldest or newest frame. Only the oldest frame will be popped off the underlying queue of frames. (optional).
            copyData (bool): Selects whether to return a copy of the numpy frame which points to a new buffer, or the original numpy frame which points to the
                             buffer used directly by PVCAM. Disabling this copy is not recommended for most situations. Refer to PyVCAM Wrapper.md for more details. (optional).

            None
        Returns:
            A dictionary with the frame containing available meta data and 2D np.array pixel data, frames per second and frame count.
        """

        frame, fps, frame_count = pvc.get_frame(self.__handle, self.__rois, self.__dtype.num, timeout_ms, oldestFrame)

        num_rois = len(frame['pixel_data'])
        if copyData:
            frameTmp = {'pixel_data': [None] * num_rois}
            for roi_index in range(num_rois):
                frameTmp['pixel_data'][roi_index] = np.copy(frame['pixel_data'][roi_index])

            if 'meta_data' in frame.keys():
                frameTmp['meta_data'] = deepcopy(frame['meta_data'])

            frame = frameTmp

        # If using a single ROI, remove list container
        if num_rois == 1:
            frame['pixel_data'] = frame['pixel_data'][0]

        return frame, fps, frame_count

    def get_frame(self, exp_time=None, timeout_ms=WAIT_FOREVER):
        """Calls the pvc.get_frame function with the current camera settings.

        Parameter:
            exp_time (int): The exposure time (optional).
        Returns:
            A 2D np.array containing the pixel data from the captured frame.
        """
        self.start_seq(exp_time=exp_time, num_frames=1)
        frame, fps, frame_count = self.poll_frame(timeout_ms=timeout_ms)
        self.finish()

        return frame['pixel_data']

    def get_sequence(self, num_frames, exp_time=None, timeout_ms=WAIT_FOREVER, interval=None):
        """Calls the pvc.get_frame function with the current camera settings in
            rapid-succession for the specified number of frames

        Parameter:
            num_frames (int): Number of frames to capture in the sequence
            exp_time (int): The exposure time (optional)
            interval (int): The time in milliseconds to wait between captures
        Returns:
            A 3D np.array containing the pixel data from the captured frames.
        """

        if len(self.__rois) > 1:
            raise ValueError('get_sequence does not support multi-roi captures')

        shape = self.__rois[0].shape
        stack = np.empty((num_frames, shape[1], shape[0]), dtype=self.__dtype)

        for i in range(num_frames):
            stack[i] = self.get_frame(exp_time=exp_time, timeout_ms=timeout_ms)

            if isinstance(interval, int):
                time.sleep(interval / 1000)

        return stack

    def get_vtm_sequence(self, time_list, exp_res, num_frames, timeout_ms=WAIT_FOREVER, interval=None):
        """Calls the pvc.get_frame function within a loop, setting vtm expTime
            between each capture.

        Parameter:
            time_list (list of ints): List of vtm timings
            exp_res (int): vtm exposure time resolution (0:mili, 1:micro)
            num_frames (int): Number of frames to capture in the sequence
            interval (int): The time in milliseconds to wait between captures
        Returns:
            A 3D np.array containing the pixel data from the captured sequence.
        """
        old_res = self.exp_res
        self.exp_res = exp_res

        if len(self.__rois) > 1:
            raise ValueError('get_vtm_sequence does not support multi-roi captures')

        shape = self.__rois[0].shape
        stack = np.empty((num_frames, shape[1], shape[0]), dtype=self.__dtype)

        for i in range(num_frames):
            exp_time = time_list[i]
            try:
                self.vtm_exp_time = exp_time
                stack[i] = self.get_frame(exp_time=self.vtm_exp_time, timeout_ms=timeout_ms)
            except Exception:
                raise ValueError('Could not collect vtm frame')

            if isinstance(interval, int):
                time.sleep(interval / 1000)

        self.exp_res = old_res
        return stack

    def start_live(self, exp_time=None, buffer_frame_count=16, stream_to_disk_path=None):
        """Calls the pvc.start_live function to setup a circular buffer acquisition.

        Parameter:
            exp_time (int): The exposure time (optional).
        Returns:
            None
        """
        self._set_dtype()

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
                raise ValueError('Invalid directory for stream to disk: ' + directory)

        self.__acquisition_mode = 'Live'
        self.__exposure_bytes = pvc.start_live(self.__handle, self.__rois, exp_time, self.__mode, buffer_frame_count, stream_to_disk_path)

    def start_seq(self, exp_time=None, num_frames=1):
        """Calls the pvc.start_seq function to setup a non-circular buffer acquisition.

        Parameter:
            exp_time (int): The exposure time (optional).
        Returns:
            None
        """
        self._set_dtype()

        if not isinstance(exp_time, int):
            exp_time = self.exp_time

        self.__acquisition_mode = 'Sequence'
        self.__exposure_bytes = pvc.start_seq(self.__handle, self.__rois, exp_time, self.__mode, num_frames)

    def finish(self):
        """Ends a previously started live or sequence acquisition.

        Parameter:
            None
        Returns:
            None
        """
        if self.__acquisition_mode == 'Live':
            pvc.stop_live(self.__handle)
        elif self.__acquisition_mode == 'Sequence':
            pvc.finish_seq(self.__handle)

        self.__acquisition_mode = None
        return

    def abort(self):
        """Calls the pvc.abort function that aborts acquisition.

        Parameter:
            None
        Returns:
            None
        """
        return pvc.abort(self.__handle)

    def sw_trigger(self):
        """Performs a SW trigger. This trigger behaves analogously to a HW external trigger. Will throw an exception if trigger fails.

        Parameter:
            None
        Returns:
            None
        """

        pvc.sw_trigger(self.__handle)

    def set_post_processing_param(self, feature_name, param_name, value):
        """Sets the value of a post processing parameter.

        Parameter:
            Feature name and parameter name as specified in post_processing_table
        Returns:
            None
        """

        if feature_name in self.__post_processing_table.keys():
            if param_name in self.__post_processing_table[feature_name].keys():
                pp_param = self.__post_processing_table[feature_name][param_name]

                if pp_param['param_min'] <= value <= pp_param['param_max']:
                    self.set_param(const.PARAM_PP_INDEX, pp_param['feature_index'])
                    self.set_param(const.PARAM_PP_PARAM_INDEX, pp_param['param_index'])
                    self.set_param(const.PARAM_PP_PARAM, value)
                else:
                    raise AttributeError('Could not set post processing param. Value ' + str(value) + ' out of range (' + str(pp_param['param_min']) + ', ' + str(pp_param['param_max']) + ')')
            else:
                raise AttributeError('Could not set post processing param. param_name not found')
        else:
            raise AttributeError('Could not set post processing param. feature_name not found')

    def get_post_processing_param(self, feature_name, param_name):
        """Gets the current value of a post processing parameter.

        Parameter:
            Feature name and parameter name as specified in post_processing_table
        Returns:
            Value of specified post processing parameter
        """

        if feature_name in self.__post_processing_table.keys():
            if param_name in self.__post_processing_table[feature_name].keys():
                pp_param = self.__post_processing_table[feature_name][param_name]
                self.set_param(const.PARAM_PP_INDEX, pp_param['feature_index'])
                self.set_param(const.PARAM_PP_PARAM_INDEX, pp_param['param_index'])
                return self.get_param(const.PARAM_PP_PARAM)
            else:
                raise AttributeError('Could not set post processing param. param_name not found')
        else:
            raise AttributeError('Could not set post processing param. feature_name not found')

    def _set_dtype(self):
        bit_depth = self.bit_depth
        bytes_per_pixel = int(np.ceil(bit_depth / 8))
        dtypeKind = 'u{}'.format(bytes_per_pixel)
        self.__dtype = np.dtype(dtypeKind)

    ### Getters/Setters below ###

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
    def prog_scan_modes(self):
        return self.__prog_scan_modes

    @property
    def prog_scan_dirs(self):
        return self.__prog_scan_dirs

    @property
    def driver_version(self):
        dd_ver = self.get_param(const.PARAM_DD_VERSION)
        # The device driver version is returned as a highly formatted 16 bit
        # integer where the first 8 bits are the major version, bits 9-12 are
        # the minor version, and bits 13-16 are the build number. Uses of masks
        # and bit shifts are required to extract the full version number.
        return '{}.{}.{}'.format(dd_ver & 0xff00 >> 8,
                                 dd_ver & 0x00f0 >> 4,
                                 dd_ver & 0x000f)

    @property
    def cam_fw(self):
        return pvc.get_cam_fw_version(self.__handle)

    @property
    def chip_name(self):
        return self.get_param(const.PARAM_CHIP_NAME)

    @property
    def sensor_size(self):
        return (self.get_param(const.PARAM_SER_SIZE),
                self.get_param(const.PARAM_PAR_SIZE))

    @property
    def serial_no(self):
        # HACK: cytocam fix for messed up serial numbers
        try:
            serial_no = self.get_param(const.PARAM_HEAD_SER_NUM_ALPHA)
            return serial_no
        except:
            return 'N/A'

    @property
    def bit_depth(self):
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
    def readout_port(self, value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        num_ports = self.get_param(const.PARAM_READOUT_PORT, const.ATTR_COUNT)

        if value >= num_ports:
            raise ValueError('{} only supports '
                             '{} readout ports.'.format(self, num_ports))
        self.set_param(const.PARAM_READOUT_PORT, value)

    @property
    def speed_table_index(self):
        return self.get_param(const.PARAM_SPDTAB_INDEX)

    @speed_table_index.setter
    def speed_table_index(self, value):
        num_entries = self.get_param(const.PARAM_SPDTAB_INDEX,
                                     const.ATTR_COUNT)
        if value >= num_entries:
            raise ValueError('{} only supports '
                             '{} speed entries'.format(self, num_entries))
        self.set_param(const.PARAM_SPDTAB_INDEX, value)

    @property
    def trigger_table(self):
        # Returns a dictionary containing information about the last capture.
        # Note some features are camera specific.

        if self.exp_res == 1:
            exp = str(self.last_exp_time) + ' μs'
        else:
            exp = str(self.last_exp_time) + ' ms'

        try:
            read = str(self.readout_time) + ' μs'
        except AttributeError:
            read = 'N/A'

        # If the camera has clear time, then it has pre and post trigger delays
        try:
            clear = str(self.clear_time) + ' ns'
            pre = str(self.pre_trigger_delay) + ' ns'
            post = str(self.post_trigger_delay) + ' ns'
        except:
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
        if not (min_gain <= value <= max_gain):
            raise ValueError("Invalid value: {} - {} only supports gain "
                             "indicies from {} - {}.".format(value, self, min_gain, max_gain))
        self.set_param(const.PARAM_GAIN_INDEX, value)

    @property
    def binning(self):
        return self.bin_x, self.bin_y

    @binning.setter
    def binning(self, value):
        if isinstance(value, tuple):
            self.bin_x = value[0]
            self.bin_y = value[1]
        else:
            self.binning = (value, value)

    @property
    def bin_x(self):
        return self.__rois[0].sbin

    @bin_x.setter
    def bin_x(self, value):
        # Will raise ValueError if incompatible binning is set
        if value in self.read_enum(const.PARAM_BINNING_SER).values():
            for roi in self.__rois:
                roi.sbin = value
            return

        raise ValueError('{} only supports {} binnings'.format(self,
                                                               self.read_enum(const.PARAM_BINNING_SER).items()))

    @property
    def bin_y(self):
        return self.__rois[0].pbin

    @bin_y.setter
    def bin_y(self, value):
        # Will raise ValueError if incompatible binning is set
        if value in self.read_enum(const.PARAM_BINNING_PAR).values():
            for roi in self.__rois:
                roi.pbin = value
            return

        raise ValueError('{} only supports {} binnings'.format(self,
                                                               self.read_enum(const.PARAM_BINNING_PAR).items()))

    def shape(self, roi_index=0):
        return self.__rois[roi_index].shape

    @property
    def last_exp_time(self):
        return self.get_param(const.PARAM_EXPOSURE_TIME)

    @property
    def exp_res(self):
        return self.get_param(const.PARAM_EXP_RES)

    @exp_res.setter
    def exp_res(self, keyOrValue):
        # Will raise ValueError if provided with an unrecognized key.
        value = self.__exp_resolutions[keyOrValue] if isinstance(keyOrValue, str) else keyOrValue

        # Verify value is in range by attempting to look-up the key
        key = self.__exp_resolutions[value]

        self.set_param(const.PARAM_EXP_RES, value)

    @property
    def exp_res_index(self):
        return self.get_param(const.PARAM_EXP_RES_INDEX)

    @property
    def exp_time(self):
        # TODO: Testing
        return self.__exp_time

    @exp_time.setter
    def exp_time(self, value):
        min_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MIN)
        max_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MAX)

        if not value in range(min_exp_time, max_exp_time + 1):
            raise ValueError("Invalid value: {} - {} only supports exposure "
                             "times between {} and {}".format(value, self,
                                                              min_exp_time,
                                                              max_exp_time))
        self.__exp_time = value

    @property
    def exp_mode(self):
        return self.get_param(const.PARAM_EXPOSURE_MODE)

    @exp_mode.setter
    def exp_mode(self, keyOrValue):
        # Will raise ValueError if provided with an unrecognized key.
        self.__exp_mode = self.__exp_modes[keyOrValue] if isinstance(keyOrValue, str) else keyOrValue

        # Verify value is in range by attempting to look-up the key
        key = self.__exp_modes[self.__exp_mode]

        self._update_mode()

    @property
    def exp_out_mode(self):
        return self.get_param(const.PARAM_EXPOSE_OUT_MODE)

    @exp_out_mode.setter
    def exp_out_mode(self, keyOrValue):
        # Will raise ValueError if provided with an unrecognized key.
        self.__exp_out_mode = self.__exp_out_modes[keyOrValue] if isinstance(keyOrValue, str) else keyOrValue

        # Verify value is in range by attempting to look-up the key
        key = self.__exp_out_modes[self.__exp_out_mode]

        self._update_mode()

    @property
    def vtm_exp_time(self):
        return self.get_param(const.PARAM_EXP_TIME)

    @vtm_exp_time.setter
    def vtm_exp_time(self, value):
        min_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MIN)
        max_exp_time = self.get_param(const.PARAM_EXPOSURE_TIME, const.ATTR_MAX)

        if not value in range(min_exp_time, max_exp_time + 1):
            raise ValueError("Invalid value: {} - {} only supports exposure "
                             "times between {} and {}".format(value, self,
                                                              min_exp_time,
                                                              max_exp_time))
        self.set_param(const.PARAM_EXP_TIME, value)

    @property
    def clear_mode(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_CLEAR_MODE)

    @clear_mode.setter
    def clear_mode(self, keyOrValue):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting. Will raise ValueError if provided with an unrecognized key.
        value = self.__clear_modes[keyOrValue] if isinstance(keyOrValue, str) else keyOrValue

        # Verify value is in range by attempting to look-up the key
        key = self.__clear_modes[value]

        self.set_param(const.PARAM_CLEAR_MODE, value)

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
        try:
            self.set_param(const.PARAM_TEMP_SETPOINT, value)
        except RuntimeError:
            min_temp = self.get_param(const.PARAM_TEMP_SETPOINT, const.ATTR_MIN)
            max_temp = self.get_param(const.PARAM_TEMP_SETPOINT, const.ATTR_MAX)
            raise ValueError("Invalid temp {} : Valid temps are in the range {} "
                             "- {}.".format(value, min_temp, max_temp))

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
    def centroids_mode(self, keyOrValue):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting. Will raise ValueError if
        # provided with an unrecognized key
        value = self.__centroids_modes[keyOrValue] if isinstance(keyOrValue, str) else keyOrValue

        # Verify value is in range by attempting to look-up the key
        key = self.__centroids_modes[value]

        self.set_param(const.PARAM_CENTROIDS_MODE, value)

    @property
    def scan_line_time(self):
        return self.get_param(const.PARAM_SCAN_LINE_TIME)

    @property
    def prog_scan_mode(self):
        # Camera specific setting: Will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_SCAN_MODE)

    @prog_scan_mode.setter
    def prog_scan_mode(self, keyOrValue):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting. Will raise ValueError if
        # provided with an unrecognized key
        value = self.__prog_scan_modes[keyOrValue] if isinstance(keyOrValue, str) else keyOrValue

        # Verify value is in range by attempting to look-up the key
        key = self.__prog_scan_modes[value]

        self.set_param(const.PARAM_SCAN_MODE, value)

    @property
    def prog_scan_dir(self):
        # Camera specific setting: Will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_SCAN_DIRECTION)

    @prog_scan_dir.setter
    def prog_scan_dir(self, keyOrValue):
        # Camera specific setting. Will raise AttributeError if called with a
        # camera that does not support this setting. Will raise ValueError if
        # provided with an unrecognized key or value
        value = self.__prog_scan_dirs[keyOrValue] if isinstance(keyOrValue, str) else keyOrValue

        # Verify value is in range by attempting to look-up the key
        key = self.__prog_scan_dirs[value]

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
    def prog_scan_width(self):
        return self.get_param(const.PARAM_SCAN_WIDTH)

    @prog_scan_width.setter
    def prog_scan_width(self, value):
        self.set_param(const.PARAM_SCAN_WIDTH, value)

    @property
    def meta_data_enabled(self):
        return self.get_param(const.PARAM_METADATA_ENABLED)

    @meta_data_enabled.setter
    def meta_data_enabled(self, value):
        self.set_param(const.PARAM_METADATA_ENABLED, value)
