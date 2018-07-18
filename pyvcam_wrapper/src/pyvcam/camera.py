from pyvcam import pvc
from pyvcam import constants as const

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

        __binning(tuple): Tuple 2 integers representing the serial and parallel binning.
        __roi(tuple): Tuple of 4 integers representing the region-of-interest.
        __shape(tuple): Tuple of 2 integers representing the image dimensions.
    """

    def __init__(self, name):
        """NOTE: CALL Camera.detect_camera() to get a camera object."""
        self.__name = name
        self.__handle = -1
        self.__is_open = False

        # Memory for live circular buffer
        self.__exposure_bytes = None

        # Exposure Settings
        self.__mode = None
        self.__exp_time = 0

        # Image metadata
        self.__binning = (1, 1)
        self.__roi = None
        self.__shape = None

    def __repr__(self):
        return self.name

    @classmethod
    def detect_camera(cls):
        """Detects and creates a new Camera object.

        Returns:
            A Camera object.
        """
        cam_count = 0
        total = pvc.get_cam_total()
        while cam_count < total:
            try:
                yield Camera(pvc.get_cam_name(cam_count))
                cam_count += 1
            except RuntimeError:
                raise RuntimeError('Failed to create a detected camera.')

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

        # Set ROI to full frame
        self.roi = (0, self.sensor_size[0], 0, self.sensor_size[1])

        # Setup correct mode
        self.__exp_mode = self.get_param(const.PARAM_EXPOSURE_MODE)
        if self.check_param(const.PARAM_EXPOSE_OUT_MODE):
            self.__exp_out_mode = self.get_param(const.PARAM_EXPOSE_OUT_MODE)
        else:
            self.__exp_out_mode = 0

        self.__mode = self.__exp_mode | self.__exp_out_mode

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
        return pvc.get_param(self.handle, param_id, param_attr)

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

        Note:
            The camera objects pp_table never changes at this point. Maybe in
            the future there will be setters and getters for each
            post-processing feature.

        Returns:
            None
        """
        try:
            pvc.reset_pp(self.__handle)
        except:
            raise RuntimeError('Failed to reset post-processing settings.')

    def calculate_reshape(self):
        """Calculates the shape of the output frame based on serial/ parallel
           binning and ROI. This function should only be called internally
           whenever the binning or roi is modifed.

        Side Effect(s):
            - Changes self.__shape

        Returns:
            None
        """
        area = (self.__roi[1] - self.__roi[0], self.__roi[3] - self.__roi[2])
        self.__shape = (int(area[0]/ self.bin_x), int(area[1]/self.bin_y))

    def update_mode(self):
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

    def get_frame(self, exp_time=None):
        """Calls the pvc.get_frame function with the current camera settings.

        Parameter:
            exp_time (int): The exposure time (optional).
        Returns:
            A 2D np.array containing the pixel data from the captured frame.
        """
        # If there is an roi specified, use that, otherwise, use the full sensor
        # size.
        x_start, x_end, y_start, y_end = self.__roi

        if not isinstance(exp_time, int):
            exp_time = self.exp_time

        tmpFrame = pvc.get_frame(self.__handle, x_start, x_end - 1, self.bin_x,
                                    y_start, y_end - 1, self.bin_y, exp_time,
                                    self.__mode).reshape(self.__shape[1], self.__shape[0])
        return np.copy(tmpFrame)

    def get_sequence(self, num_frames, exp_time=None, interval=None):
        """Calls the pvc.get_frame function with the current camera settings in
            rapid-succession for the specified number of frames

        Parameter:
            num_frames (int): Number of frames to capture in the sequence
            exp_time (int): The exposure time (optional)
            interval (int): The time in milliseconds to wait between captures
        Returns:
            A 3D np.array containing the pixel data from the captured frames.
        """
        x_start, x_end, y_start, y_end = self.__roi

        if not isinstance(exp_time, int):
            exp_time = self.exp_time

        stack = np.empty((num_frames, self.__shape[1], self.__shape[0]), dtype=np.uint16)

        for i in range(num_frames):
            stack[i] = pvc.get_frame(self.__handle, x_start, x_end - 1, self.bin_x,
                                    y_start, y_end - 1, self.bin_y, exp_time,
                                    self.__mode).reshape(self.__shape[1],
                                    self.__shape[0])

            if isinstance(interval, int):
                time.sleep(interval/1000)

        return stack

    def get_vtm_sequence(self, time_list, exp_res, num_frames, interval=None):
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
        # Checking to see if all timings are valid (max val is uint16 max)
        for t in time_list:
            if t not in range(2**16):
                raise ValueError("Invalid value: {} - {} only supports vtm times "
                                    "between {} and {}".format(t, self, 0, 2**16))

        x_start, x_end, y_start, y_end = self.__roi

        old_res = self.exp_res
        self.exp_res = exp_res

        stack = np.empty((num_frames, self.shape[1], self.shape[0]), dtype=np.uint16)

        t = iter(time_list) # using time_list iterator to loop through all values
        for i in range(num_frames):
            try:
                self.vtm_exp_time = next(t)
            except:
                t = iter(time_list)
                self.vtm_exp_time = next(t)

            stack[i] = pvc.get_frame(self.__handle, x_start, x_end - 1, self.bin_x,
                                     y_start, y_end - 1, self.bin_y, self.exp_time,
                                     self.__mode).reshape(self.__shape[1], self.__shape[0])

            if isinstance(interval, int):
                time.sleep(interval/ 1000)

        self.exp_res = old_res
        return stack

    def start_live(self, exp_time=None):
        """Calls the pvc.start_live function to setup a circular buffer acquisition.

        Parameter:
            exp_time (int): The exposure time (optional).
        Returns:
            None
        """
        x_start, x_end, y_start, y_end = self.__roi

        if not isinstance(exp_time, int):
            exp_time = self.exp_time

        self.__exposure_bytes = pvc.start_live(self.__handle, x_start, x_end - 1,
                                               self.bin_x, y_start, y_end - 1,
                                               self.bin_x, exp_time, self.__mode)

    def stop_live(self):
        """Calls the pvc.stop_live function that ends the acquisition.

        Parameter:
            None
        Returns:
            None
        """
        return pvc.stop_live(self.__handle)

    def get_live_frame(self):
        """Calls the pvc.get_live_frame function with the current camera settings.

        Parameter:
            None
        Returns:
            A 2D np.array containing the pixel data from the captured frame.
        """
        return pvc.get_live_frame(self.__handle, self.__exposure_bytes).reshape(
                                                 self.__shape[1], self.__shape[0])

    def start_live_cb(self, exp_time=None):
        """Calls the pvc.start_live_cb function to setup a circular buffer acquisition with callbacks.

        Parameter:
            exp_time (int): The exposure time (optional).
        Returns:
            None
        """
        x_start, x_end, y_start, y_end = self.__roi

        if not isinstance(exp_time, int):
            exp_time = self.exp_time

        self.__exposure_bytes = pvc.start_live_cb(self.__handle, x_start, x_end - 1,
                                                  self.bin_x, y_start, y_end - 1,
                                                  self.bin_x, exp_time, self.__mode)
        return self.__exposure_bytes

    def get_live_frame_cb(self):
        """Calls the pvc.get_live_frame_cb function.

        Parameter:
            None
        Returns:
            A 2D np.array containing the pixel data from the captured frame.
        """
        frame, fps = pvc.get_live_frame_cb(self.__handle, self.__exposure_bytes)

        return frame.reshape(self.__shape[1], self.__shape[0]), fps


    ### Getters/Setters below ###
    @property
    def name(self):
        return self.__name

    @property
    def handle(self):
        return self.__handle

    @property
    def is_open(self):
        return self.__is_open

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
        #HACK: cytocam fix for messed up serial numbers
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
    def speed_table(self):
        # Iterate through all available camera ports and their readout speeds
        # to create the camera speed table.
        speed_table = {}
        for port in self.read_enum(const.PARAM_READOUT_PORT).values():
            speed_table[port] = []
            self.readout_port = port
            num_speeds = self.get_param(const.PARAM_SPDTAB_INDEX,
                                        const.ATTR_COUNT)
            for i in range(num_speeds):
                self.speed_table_index = i
                gain_min = self.get_param(const.PARAM_GAIN_INDEX,
                                          const.ATTR_MIN)
                gain_max = self.get_param(const.PARAM_GAIN_INDEX,
                                          const.ATTR_MAX)
                gain_increment = self.get_param(const.PARAM_GAIN_INDEX,
                                                const.ATTR_INCREMENT)
                readout_option = {
                    "Speed Index": i,
                    "Pixel Time": self.pix_time,
                    "Bit Depth": self.bit_depth,
                    "Gains": range(gain_min, gain_max + 1, gain_increment)
                }
                speed_table[port].append(readout_option)

        # Resseting speed table back to default
        self.readout_port = 0
        self.speed_table_index = 0

        return speed_table

    @property
    def pp_table(self):
        # Check if camera has post-processing features. If it does, populate
        # pp_table with all features, and current settings (current val, min, max)
        pp_table = {}

        if not self.check_param(const.PARAM_PP_INDEX):
            raise AttributeError("{} does not have any post-processing features "
                                 "available.".format(self))
            return


        orig = (self.get_param(const.PARAM_PP_INDEX),
                self.get_param(const.PARAM_PP_PARAM_INDEX))

        for i in range(self.get_param(const.PARAM_PP_INDEX, const.ATTR_MAX) + 1):

            self.set_param(const.PARAM_PP_INDEX, i)
            pName = self.get_param(const.PARAM_PP_FEAT_NAME)

            pp_table[pName] = {}

            for j in range(self.get_param(const.PARAM_PP_PARAM_INDEX,
                                            const.ATTR_MAX) + 1):

                self.set_param(const.PARAM_PP_PARAM_INDEX, j)
                aName = self.get_param(const.PARAM_PP_PARAM_NAME)

                min = self.get_param(const.PARAM_PP_PARAM, const.ATTR_MIN)
                max = self.get_param(const.PARAM_PP_PARAM, const.ATTR_MAX)
                val = self.get_param(const.PARAM_PP_PARAM)

                pp_table[pName][aName] = (val, min, max)

        self.set_param(const.PARAM_PP_INDEX, orig[0])
        self.set_param(const.PARAM_PP_PARAM_INDEX, orig[1])

        return pp_table

    @property
    def trigger_table(self):
        # Returns a dictionary containing information about the last captire.
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
        max_gain = self.get_param(const.PARAM_GAIN_INDEX, const.ATTR_MAX)
        if value > max_gain or value < 1:
            raise ValueError("Invalid value: {} - {} only supports gain "
                             "indicies from 1 - {}.".format(value, self, max_gain))
        self.set_param(const.PARAM_GAIN_INDEX, value)

    @property
    def binning(self):
        return self.__binning

    @binning.setter
    def binning(self, value):
        if isinstance(value, tuple):
            self.bin_x = value[0]
            self.bin_y = value[1]
            return
        elif value in self.read_enum(const.PARAM_BINNING_SER).values():
            self.__binning = (value, value)
            self.calculate_reshape()
            return

        raise ValueError('{} only supports {} binnings'.format(self,
                                self.read_enum(const.PARAM_BINNING_SER).items()))

    @property
    def bin_x(self):
        return self.__binning[0]

    @bin_x.setter
    def bin_x(self, value):
        # Will raise ValueError if incompatible binning is set
        if value in self.read_enum(const.PARAM_BINNING_SER).values():
            self.__binning = (value, self.__binning[1])
            self.calculate_reshape()
            return

        raise ValueError('{} only supports {} binnings'.format(self,
                                self.read_enum(const.PARAM_BINNING_SER).items()))

    @property
    def bin_y(self):
        return self.__binning[1]

    @bin_y.setter
    def bin_y(self, value):
        # Will raise ValueError if incompatible binning is set
        if value in self.read_enum(const.PARAM_BINNING_PAR).values():
            self.__binning = (self.__binning[0], value)
            self.calculate_reshape()
            return

        raise ValueError('{} only supports {} binnings'.format(self,
                                self.read_enum(const.PARAM_BINNING_SER).items()))

    @property
    def roi(self):
        return self.__roi

    @roi.setter
    def roi(self, value):
        # Takes in a tuple following (x_start, x_end, y_start, y_end), and
        # sets self.__roi if valid
        if (isinstance(value, tuple) and all(isinstance(x, int) for x in value)
                and len(value) == 4):

            if (value[0] in range(0, self.sensor_size[0] + 1) and
                    value[1] in range(0, self.sensor_size[0] + 1) and
                    value[2] in range(0, self.sensor_size[1] + 1) and
                    value[3] in range(0, self.sensor_size[1] + 1)):
                self.__roi = value
                self.calculate_reshape()
                return

            else:
                raise ValueError('Invalid ROI paramaters for {}'.format(self))

        raise ValueError('{} ROI expects a tuple of 4 integers'.format(self))

    @property
    def shape(self):
        return self.__shape

    @property
    def last_exp_time(self):
        return self.get_param(const.PARAM_EXPOSURE_TIME)

    @property
    def exp_res(self):
        return self.get_param(const.PARAM_EXP_RES)

    @exp_res.setter
    def exp_res(self, value):
        # Handle the case where clear mode is passed in by name. Simply check
        # if it is a key in the resolutions dictionary in constants.py, and if
        # it is, call set_param with its associated value. Otherwise, raise
        # a ValueError informing the user which settings are valid.
        if isinstance(value, str):
            if value not in const.exp_resolutions:
                keys = [key for key in const.exp_resolutions]
                raise ValueError("Invalid mode: {0} - {1} only supports "
                                 "resolutions from the following list: {2}".format(
                                                            value, self, keys))
            self.set_param(const.PARAM_EXP_RES, const.exp_resolutions[value])

        # Handle the case where resolution is passed in by value. Check if
        # the value passed in is a valid value inside the clear_modes
        # dictionary in constants.py. If it is, call set_param with the
        # value. Otherwise, raise a ValueError informing which values
        # can be used.
        else:
            if value not in const.exp_resolutions.values():
                vals = const.exp_resolutions.values()
                raise ValueError("Invalid mode value: {0} - {1} only supports "
                                 "resolutions from {2} - {3}".format(value, self,
                                                            min(vals), max(vals)))
            self.set_param(const.PARAM_EXP_RES, value)

    @property
    def exp_res_index(self):
        return self.get_param(const.PARAM_EXP_RES_INDEX)

    @property
    def exp_time(self):
        #TODO: Testing
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
    def exp_mode(self, value):
        # Handle the case where clear mode is passed in by name. Simply check
        # if it is a key in the resolutions dictionary in constants.py, and if
        # it is, call set_param with its associated value. Otherwise, raise
        # a ValueError informing the user which settings are valid.
        if isinstance(value, str):
            if value not in const.exp_modes:
                keys = [key for key in const.exp_modes]
                raise ValueError("Invalid mode: {} - {} only supports exposure "
                                 "modes from the following list: {}".format(value,
                                                                        self, keys))

            self.__exp_mode = const.exp_modes[value]
            self.update_mode()

        # Handle the case where resolution is passed in by value. Check if
        # the value passed in is a valid value inside the clear_modes
        # dictionary in constants.py. If it is, call set_param with the
        # value. Otherwise, raise a ValueError informing which values
        # can be used.
        else:
            if value not in const.exp_modes.values():
                vals = const.exp_modes.values()
                raise ValueError("Invalid mode value: {0} - {1} only supports"
                                 "exposure modes from {2} - {3}".format(value,
                                                                    self, min(vals),
                                                                    max(vals)))

            self.__exp_mode = value
            self.update_mode()

    @property
    def exp_out_mode(self):
        return self.get_param(const.PARAM_EXPOSE_OUT_MODE)

    @exp_out_mode.setter
    def exp_out_mode(self, value):
        # Handle the case where clear mode is passed in by name. Simply check
        # if it is a key in the resolutions dictionary in constants.py, and if
        # it is, call set_param with its associated value. Otherwise, raise
        # a ValueError informing the user which settings are valid.
        if isinstance(value, str):
            if value not in const.exp_out_modes:
                keys = [key for key in const.exp_out_modes]
                raise ValueError("Invalid mode: {0} - {1} only supports exposure "
                                 "out modes from the following list: {2}".format(
                                 value, self, keys))

            self.__exp_out_mode = const.exp_out_modes[value]
            self.update_mode()
        # Handle the case where resolution is passed in by value. Check if
        # the value passed in is a valid value inside the clear_modes
        # dictionary in constants.py. If it is, call set_param with the
        # value. Otherwise, raise a ValueError informing which values
        # can be used.
        else:
            if value not in const.exp_out_modes.values():
                vals = const.exp_out_modes.values()
                raise ValueError("Invalid mode value: {0} - {1} only supports "
                                 "exposure out modes from {2} - {3}".format(value,
                                 self, min(vals), max(vals)))

            self.__exp_out_mode = value
            self.update_mode()

    @property
    def vtm_exp_time(self):
        return self.get_param(const.PARAM_EXP_TIME)

    @vtm_exp_time.setter
    def vtm_exp_time(self, value):
        if not value in range(2**16):
            raise ValueError("Invalid value: {} - {} only supports exposure "
                             "times between {} and {}".format(value, self, 0, 2**16))
        self.set_param(const.PARAM_EXP_TIME, value)

    @property
    def clear_mode(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        # Find the current int32 value corresponding to the clear mode
        # of the camera, then reverse the key value pairs of the clear_modes
        # dictionary inside constants.py since it maps the string name of
        # the clear mode to its PVCAM value. This will allow for "reverse"
        # lookup, meaning that it will return the string name given its
        # integer value.
        return self.get_param(const.PARAM_CLEAR_MODE)
        # curr_mode_val = self.get_param(const.PARAM_CLEAR_MODE)
        # return {v: k for k, v in const.clear_modes.items()}[curr_mode_val]

    @clear_mode.setter
    def clear_mode(self, value):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        # Handle the case where clear mode is passed in by name. Simply check
        # if it is a key in the clear_modes dictionary in constants.py, and if
        # it is, call set_param with its associated value. Otherwise, raise
        # a ValueError informing the user which settings are valid.
        if isinstance(value, str):
            if value not in const.clear_modes:
                keys = [key for key in const.clear_modes]
                raise ValueError("Invalid mode: {0} - {1} "
                                 "only supports clear modes "
                                 "from the following list: {2}".format(value,
                                                                       self,
                                                                       keys))
            self.set_param(const.PARAM_CLEAR_MODE, const.clear_modes[value])
        # Handle the case where clear mode is passed in by value. Check if
        # the value passed in is a valid value inside the clear_modes
        # dictionary in constants.py. If it is, call set_param with the
        # value. Otherwise, raise a ValueError informing which values
        # can be used.
        else:
            if value not in const.clear_modes.values():
                vals = const.clear_modes.values()
                raise ValueError("Invalid mode value: {0} "
                                 "- {1} only supports clear "
                                 "modes from the {2} - {3}".format(value,
                                                                   self,
                                                                   min(vals),
                                                                   max(vals)))
            self.set_param(const.PARAM_CLEAR_MODE, value)

    @property
    def temp(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_TEMP)/100.0

    @property
    def temp_setpoint(self):
        # Camera specific setting: will raise AttributeError if called with a
        # camera that does not support this setting.
        return self.get_param(const.PARAM_TEMP_SETPOINT)/100.0

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
