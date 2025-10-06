import unittest

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const


class CameraConstructionTests(unittest.TestCase):

    def setUp(self):
        pvc.init_pvcam()
        try:
            self.test_cam = next(Camera.detect_camera())
        except Exception as ex:
            raise unittest.SkipTest('No available camera found') from ex

    def tearDown(self):
        # if self.test_cam.is_open():
        #     self.test_cam.close()
        pvc.uninit_pvcam()

    def test_init(self):
        test_cam_name = 'test'
        test_cam = Camera(test_cam_name)
        self.assertEqual(test_cam.name, test_cam_name)
        self.assertEqual(test_cam.handle, -1)
        self.assertEqual(test_cam.is_open, False)

    def test_get_dd_version(self):
        self.test_cam.open()
        dd_ver = pvc.get_param(self.test_cam.handle,
                               const.PARAM_DD_VERSION,
                               const.ATTR_CURRENT)
        dd_ver_str = f'{(dd_ver >> 8) & 0xFF}.'\
                     f'{(dd_ver >> 4) & 0x0F}.'\
                     f'{(dd_ver >> 0) & 0x0F}'
        self.assertEqual(dd_ver_str, self.test_cam.driver_version)

    def test_get_dd_version_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.driver_version

    def test_get_binning(self):
        self.test_cam.open()
        self.assertEqual(self.test_cam.binning, (1, 1))  # Defaults to 1

    def test_get_binning_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.binning

    def test_set_binning_tuple(self):
        self.test_cam.open()
        self.test_cam.binning = (1, 1)
        self.assertEqual(self.test_cam.binning, (1, 1))  # Defaults to 1

    def test_set_binning_int(self):
        self.test_cam.open()
        self.test_cam.binning = 1
        self.assertEqual(self.test_cam.binning, (1, 1))  # Defaults to 1

    def test_set_binning_tuple_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.binning = (1, 1)

    def test_set_binning_int_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.binning = 1

    def test_get_chip_name(self):
        self.test_cam.open()
        chip_name = pvc.get_param(self.test_cam.handle,
                                  const.PARAM_CHIP_NAME,
                                  const.ATTR_CURRENT)
        self.assertEqual(chip_name, self.test_cam.chip_name)

    def test_get_chip_name_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.chip_name

    def test_get_serial_no(self):
        self.test_cam.open()
        ser_no = pvc.get_param(self.test_cam.handle,
                               const.PARAM_HEAD_SER_NUM_ALPHA,
                               const.ATTR_CURRENT)
        self.assertEqual(ser_no, self.test_cam.serial_no)

    def test_get_readout_port(self):
        self.test_cam.open()
        readout_port = pvc.get_param(self.test_cam.handle,
                                     const.PARAM_READOUT_PORT,
                                     const.ATTR_CURRENT)
        self.assertEqual(readout_port, self.test_cam.readout_port)

    def test_get_readout_port_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.readout_port

    def test_set_readout_port_by_name(self):
        self.test_cam.open()
        for name, _ in self.test_cam.readout_ports.items():
            self.test_cam.readout_port = name
            curr_readout_port = pvc.get_param(self.test_cam.handle,
                                              const.PARAM_READOUT_PORT,
                                              const.ATTR_CURRENT)
            self.assertEqual(curr_readout_port, self.test_cam.readout_port)

    def test_set_readout_port_by_value(self):
        self.test_cam.open()
        for _, value in self.test_cam.readout_ports.items():
            self.test_cam.readout_port = value
            curr_readout_port = pvc.get_param(self.test_cam.handle,
                                              const.PARAM_READOUT_PORT,
                                              const.ATTR_CURRENT)
            self.assertEqual(curr_readout_port, self.test_cam.readout_port)

    def test_set_readout_port_bad_name_fail(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.readout_port = ''

    def test_set_readout_port_bad_value_fail(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.readout_port = -1

    def test_set_readout_port_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.readout_port = 0

    def test_get_speed(self):
        self.test_cam.open()
        spdtab_index = pvc.get_param(self.test_cam.handle,
                                     const.PARAM_SPDTAB_INDEX,
                                     const.ATTR_CURRENT)
        self.assertEqual(spdtab_index, self.test_cam.speed)

    def test_get_speed_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.speed

    def test_set_speed(self):
        self.test_cam.open()
        num_entries = self.test_cam.get_param(const.PARAM_SPDTAB_INDEX,
                                              const.ATTR_COUNT)
        for i in range(num_entries):
            self.test_cam.speed = i
            self.assertEqual(i, self.test_cam.speed)

    def test_set__out_of_bounds(self):
        self.test_cam.open()
        num_entries = self.test_cam.get_param(const.PARAM_SPDTAB_INDEX,
                                              const.ATTR_COUNT)
        with self.assertRaises(ValueError):
            self.test_cam.speed = num_entries

    def test_set_speed_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.speed = 0

    def test_get_speed_name(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_SPDTAB_NAME):
            speed_name = pvc.get_param(self.test_cam.handle,
                                       const.PARAM_SPDTAB_NAME,
                                       const.ATTR_CURRENT)
        else:
            spdtab_index = pvc.get_param(self.test_cam.handle,
                                         const.PARAM_SPDTAB_INDEX,
                                         const.ATTR_CURRENT)
            speed_name = f'Speed_{spdtab_index}'
        self.assertEqual(speed_name, self.test_cam.speed_name)

    def test_get_speed_name_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.speed_name

    def test_get_bit_depth(self):
        self.test_cam.open()
        curr_bit_depth = pvc.get_param(self.test_cam.handle,
                                       const.PARAM_BIT_DEPTH,
                                       const.ATTR_CURRENT)
        self.assertEqual(curr_bit_depth, self.test_cam.bit_depth)

    def test_get_bit_depth_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.bit_depth

    def test_get_pix_time(self):
        self.test_cam.open()
        curr_pix_time = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_PIX_TIME,
                                      const.ATTR_CURRENT)
        self.assertEqual(curr_pix_time, self.test_cam.pix_time)

    def test_get_pix_time_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.pix_time

    def test_get_gain(self):
        self.test_cam.open()
        curr_gain_index = pvc.get_param(self.test_cam.handle,
                                        const.PARAM_GAIN_INDEX,
                                        const.ATTR_CURRENT)
        self.assertEqual(curr_gain_index, self.test_cam.gain)

    def test_get_gain_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.gain

    def test_set_gain(self):
        self.test_cam.open()
        max_gain = pvc.get_param(self.test_cam.handle,
                                 const.PARAM_GAIN_INDEX,
                                 const.ATTR_MAX)
        for i in range(1, max_gain + 1):
            self.test_cam.gain = i
            curr_gain_index = pvc.get_param(self.test_cam.handle,
                                            const.PARAM_GAIN_INDEX,
                                            const.ATTR_CURRENT)
            self.assertEqual(curr_gain_index, self.test_cam.gain)

    def test_set_gain_less_than_zero_fail(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.gain = -1

    def test_set_gain_more_than_max_fail(self):
        self.test_cam.open()
        max_gain = pvc.get_param(self.test_cam.handle,
                                 const.PARAM_GAIN_INDEX,
                                 const.ATTR_MAX)
        with self.assertRaises(ValueError):
            self.test_cam.gain = max_gain + 1

    def test_set_gain_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.gain = 1

    def test_get_gain_name(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_GAIN_NAME):
            gain_name = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_GAIN_NAME,
                                      const.ATTR_CURRENT)
        else:
            gain_index = pvc.get_param(self.test_cam.handle,
                                       const.PARAM_GAIN_INDEX,
                                       const.ATTR_CURRENT)
            gain_name = f'Gain_{gain_index}'
        self.assertEqual(gain_name, self.test_cam.gain_name)

    def test_get_gain_name_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.gain_name

    def test_adc_offset(self):
        self.test_cam.open()
        curr_adc_offset = pvc.get_param(self.test_cam.handle,
                                        const.PARAM_ADC_OFFSET,
                                        const.ATTR_CURRENT)
        self.assertEqual(curr_adc_offset, self.test_cam.adc_offset)

    def test_adc_offset_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.adc_offset

    def test_get_clear_mode(self):
        self.test_cam.open()
        curr_clear_mode = pvc.get_param(self.test_cam.handle,
                                        const.PARAM_CLEAR_MODE,
                                        const.ATTR_CURRENT)
        self.assertEqual(curr_clear_mode, self.test_cam.clear_mode)
        # reversed_dict = {val: key for key, val in const.clear_modes.items()}
        # self.assertEqual(reversed_dict[curr_clear_mode],
        #                  self.test_cam.clear_mode)

    def test_get_clear_mode_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.clear_mode

    def test_set_clear_mode_by_name(self):
        self.test_cam.open()
        for name, _ in self.test_cam.clear_modes.items():
            self.test_cam.clear_mode = name
            curr_clear_mode = pvc.get_param(self.test_cam.handle,
                                            const.PARAM_CLEAR_MODE,
                                            const.ATTR_CURRENT)
            self.assertEqual(curr_clear_mode, self.test_cam.clear_mode)

    def test_set_clear_mode_by_value(self):
        self.test_cam.open()
        for _, value in self.test_cam.clear_modes.items():
            self.test_cam.clear_mode = value
            curr_clear_mode = pvc.get_param(self.test_cam.handle,
                                            const.PARAM_CLEAR_MODE,
                                            const.ATTR_CURRENT)
            self.assertEqual(curr_clear_mode, self.test_cam.clear_mode)

    def test_set_clear_mode_bad_name_fail(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.clear_mode = ''

    def test_set_clear_mode_bad_value_fail(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.clear_mode = -1

    def test_set_clear_mode_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.clear_mode = 0

    def test_set_clear_mode_no_open_bad_value_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.clear_mode = -1

    def test_get_fan_speed(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_FAN_SPEED_SETPOINT):
            curr_fan_speed = pvc.get_param(self.test_cam.handle,
                                           const.PARAM_FAN_SPEED_SETPOINT,
                                           const.ATTR_CURRENT)
            self.assertEqual(curr_fan_speed, self.test_cam.fan_speed)

    def test_get_fan_speed_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.fan_speed

    def test_set_fan_speed_by_name(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_FAN_SPEED_SETPOINT):
            for name, _ in self.test_cam.fan_speeds.items():
                self.test_cam.fan_speed = name
                curr_fan_speed = pvc.get_param(self.test_cam.handle,
                                               const.PARAM_FAN_SPEED_SETPOINT,
                                               const.ATTR_CURRENT)
                self.assertEqual(curr_fan_speed, self.test_cam.fan_speed)

    def test_set_fan_speed_by_value(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_FAN_SPEED_SETPOINT):
            for _, value in self.test_cam.fan_speeds.items():
                self.test_cam.fan_speed = value
                curr_fan_speed = pvc.get_param(self.test_cam.handle,
                                               const.PARAM_FAN_SPEED_SETPOINT,
                                               const.ATTR_CURRENT)
                self.assertEqual(curr_fan_speed, self.test_cam.fan_speed)

    def test_set_fan_speed_bad_name_fail(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_FAN_SPEED_SETPOINT):
            with self.assertRaises(ValueError):
                self.test_cam.fan_speed = ''

    def test_set_fan_speed_bad_value_fail(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_FAN_SPEED_SETPOINT):
            with self.assertRaises(ValueError):
                self.test_cam.fan_speed = -1

    def test_set_fan_speed_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.fan_speed = 0

    def test_get_temp(self):
        self.test_cam.open()
        curr_temp = pvc.get_param(self.test_cam.handle,
                                  const.PARAM_TEMP,
                                  const.ATTR_CURRENT)
        new_temp = self.test_cam.temp
        # Less than +/- 0.5 deg C variation of temperature between calls
        self.assertLessEqual(abs(curr_temp / 100.0 - new_temp), 0.5)

    def test_get_temp_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.temp

    def test_get_temp_setpoint(self):
        self.test_cam.open()
        curr_temp_setpoint = pvc.get_param(self.test_cam.handle,
                                           const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_CURRENT)
        self.assertEqual(curr_temp_setpoint / 100.0, self.test_cam.temp_setpoint)

    def test_get_temp_setpoint_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.temp_setpoint

    def test_set_temp_setpoint(self):
        self.test_cam.open()
        min_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_MIN)
        max_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_MAX)
        min_temp_deg_c = int(min_temp / 100.0)
        max_temp_deg_c = int(max_temp / 100.0)
        for i in range(max_temp_deg_c, min_temp_deg_c - 1, -1):
            self.test_cam.temp_setpoint = i
            self.assertEqual(self.test_cam.temp_setpoint, i)

    def test_set_temp_too_high_fail(self):
        self.test_cam.open()
        max_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_MAX)
        with self.assertRaises(ValueError):
            self.test_cam.temp_setpoint = (max_temp + 1) / 100.0

    def test_set_temp_too_low_fail(self):
        self.test_cam.open()
        min_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_MIN)
        with self.assertRaises(ValueError):
            self.test_cam.temp_setpoint = (min_temp - 1) / 100.0

    def test_set_temp_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.temp_setpoint = 0

    def test_get_exp_res(self):
        self.test_cam.open()
        curr_exp_res_index = pvc.get_param(self.test_cam.handle,
                                           const.PARAM_EXP_RES_INDEX,
                                           const.ATTR_CURRENT)
        self.assertEqual(curr_exp_res_index, self.test_cam.exp_res_index)

    def test_get_exp_res_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.exp_res

    def test_set_exp_res_by_name(self):
        self.test_cam.open()
        for i in range(self.test_cam.exp_res_index):
            self.test_cam.exp_res = self.test_cam.exp_resolutions[i]
            curr_exp_res = pvc.get_param(self.test_cam.handle,
                                         const.PARAM_EXP_RES,
                                         const.ATTR_CURRENT)
            self.assertEqual(curr_exp_res, self.test_cam.exp_res)

    def test_set_exp_res_bad_name_fail(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.exp_res = ""

    def test_set_exp_res_bad_value_fail(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.exp_res = -1

    def test_set_exp_res_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.exp_res = 0

    def test_get_sensor_size(self):
        self.test_cam.open()
        rows = pvc.get_param(self.test_cam.handle,
                             const.PARAM_SER_SIZE,
                             const.ATTR_CURRENT)
        cols = pvc.get_param(self.test_cam.handle,
                             const.PARAM_PAR_SIZE,
                             const.ATTR_CURRENT)
        self.assertEqual((rows, cols), self.test_cam.sensor_size)

    def test_get_sensor_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.sensor_size

    def test_get_exp_mode(self):
        self.test_cam.open()
        curr_exp_mode = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_EXPOSURE_MODE,
                                      const.ATTR_CURRENT)
        self.assertEqual(curr_exp_mode, self.test_cam.exp_mode)

    def test_get_exp_mode_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            _ = self.test_cam.exp_mode

    def test_get_live_roi(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_ROI):
            self.test_cam.get_frame()  # Just to apply all full sensor ROI
            curr_roi = pvc.get_param(self.test_cam.handle,
                                     const.PARAM_ROI,
                                     const.ATTR_CURRENT)
            self.assertEqual(curr_roi, self.test_cam.live_roi)
            self.assertEqual(curr_roi['s2'], self.test_cam.sensor_size[0] - 1)
            self.assertEqual(curr_roi['p2'], self.test_cam.sensor_size[1] - 1)

    def test_set_live_roi_idle(self):
        self.test_cam.open()
        if (
            pvc.check_param(self.test_cam.handle, const.PARAM_ROI) and
            pvc.get_param(self.test_cam.handle, const.PARAM_ROI, const.ATTR_LIVE)
        ):
            self.test_cam.get_frame()  # Just to apply all full sensor ROI
            new_roi = self.test_cam.rois[0]
            # Set the same ROI back, should always succeed, even outside acq.
            self.test_cam.live_roi = new_roi
            curr_roi = pvc.get_param(self.test_cam.handle,
                                     const.PARAM_ROI,
                                     const.ATTR_CURRENT)
            self.assertEqual(curr_roi, self.test_cam.live_roi)

            new_roi.p2 = ((new_roi.p2 + 1) // 2) - 1  # Change to upper half
            with self.assertRaises(RuntimeError):
                # Set the ROI back, fails outside acq.
                self.test_cam.live_roi = new_roi

    def test_set_live_roi_active(self):
        self.test_cam.open()
        if (
            pvc.check_param(self.test_cam.handle, const.PARAM_ROI) and
            pvc.get_param(self.test_cam.handle, const.PARAM_ROI, const.ATTR_LIVE)
        ):
            self.test_cam.get_frame()  # Just to apply all full sensor ROI
            new_roi = self.test_cam.rois[0]
            new_roi.p2 = ((new_roi.p2 + 1) // 2) - 1  # Change to upper half
            # Start live acq. to apply live ROI, then stop immediately
            self.test_cam.start_live()
            self.test_cam.live_roi = new_roi
            self.test_cam.finish()
            curr_roi = pvc.get_param(self.test_cam.handle,
                                     const.PARAM_ROI,
                                     const.ATTR_CURRENT)
            self.assertEqual(curr_roi, self.test_cam.live_roi)

    def test_get_smart_stream_mode_enabled(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_SMART_STREAM_MODE_ENABLED):
            cur_value = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_SMART_STREAM_MODE_ENABLED,
                                      const.ATTR_CURRENT)
            self.assertEqual(cur_value, self.test_cam.smart_stream_mode_enabled)

    def test_set_smart_stream_mode_enabled(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_SMART_STREAM_MODE_ENABLED):
            for new_value in (True, False):
                self.test_cam.smart_stream_mode_enabled = new_value
                cur_value = pvc.get_param(self.test_cam.handle,
                                          const.PARAM_SMART_STREAM_MODE_ENABLED,
                                          const.ATTR_CURRENT)
                self.assertEqual(new_value, cur_value)
                self.assertEqual(new_value, self.test_cam.smart_stream_mode_enabled)

    def test_get_smart_stream_mode(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_SMART_STREAM_MODE):
            cur_value = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_SMART_STREAM_MODE,
                                      const.ATTR_CURRENT)
            self.assertEqual(cur_value, self.test_cam.smart_stream_mode)

    def test_set_smart_stream_mode(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_SMART_STREAM_MODE):
            min_value = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_SMART_STREAM_MODE,
                                      const.ATTR_MIN)
            max_value = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_SMART_STREAM_MODE,
                                      const.ATTR_MAX)
            for new_value in range(min_value, max_value + 1):
                self.test_cam.smart_stream_mode = new_value
                cur_value = pvc.get_param(self.test_cam.handle,
                                          const.PARAM_SMART_STREAM_MODE,
                                          const.ATTR_CURRENT)
                self.assertEqual(new_value, cur_value)
                self.assertEqual(new_value, self.test_cam.smart_stream_mode)

    def test_get_smart_stream_exp_params(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_SMART_STREAM_EXP_PARAMS):
            cur_value = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_SMART_STREAM_EXP_PARAMS,
                                      const.ATTR_CURRENT)
            self.assertEqual(cur_value, self.test_cam.smart_stream_exp_params)

    def test_set_smart_stream_exp_params(self):
        self.test_cam.open()
        if pvc.check_param(self.test_cam.handle, const.PARAM_SMART_STREAM_EXP_PARAMS):
            # ATTR_MAX returns a list too, only the list length is important
            max_value = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_SMART_STREAM_EXP_PARAMS,
                                      const.ATTR_MAX)
            # Known bug, cameras report 16 but accept 15 values only
            max_value_count = len(max_value) - 1
            new_value = list(range(max_value_count))
            self.test_cam.smart_stream_exp_params = new_value
            cur_value = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_SMART_STREAM_EXP_PARAMS,
                                      const.ATTR_CURRENT)
            self.assertEqual(new_value, cur_value)
            self.assertEqual(new_value, self.test_cam.smart_stream_exp_params)


def main():
    unittest.main()


if __name__ == '__main__':
    main()
