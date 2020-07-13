import unittest
from pyvcam import pvc
from pyvcam import camera
from pyvcam import constants as const


class CameraConstructionTests(unittest.TestCase):

    def setUp(self):
        pvc.init_pvcam()
        try:
            self.test_cam = next(camera.Camera.detect_camera())
        except:
            raise unittest.SkipTest('No available camera found')

    def tearDown(self):
        # if self.test_cam.is_open():
        #     self.test_cam.close()
        pvc.uninit_pvcam()

    def test_init(self):
        test_cam_name = 'test'
        test_cam = camera.Camera(test_cam_name)
        self.assertEqual(test_cam.name, test_cam_name)
        self.assertEqual(test_cam.handle, -1)
        self.assertEqual(test_cam.is_open, False)

    def test_get_dd_version(self):
        self.test_cam.open()
        dd_ver = pvc.get_param(self.test_cam.handle,
                               const.PARAM_DD_VERSION,
                               const.ATTR_CURRENT)
        dd_ver = '{}.{}.{}'.format(dd_ver & 0xff00 >> 8,
                                   dd_ver & 0x00f0 >> 4,
                                   dd_ver & 0x000f)
        self.assertEqual(dd_ver, self.test_cam.driver_version)

    def test_get_dd_version_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.driver_version

    def test_get_bin_x(self):
        self.test_cam.open()
        try:
            self.assertEqual(self.test_cam.bin_x, 1) # Defaults to 1
        except AttributeError:
            self.skipTest("test_get_bin_x: This camera does not "
                          "support binning.")

    def test_get_bin_x_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.bin_x

    def test_get_bin_y(self):
        self.test_cam.open()
        try:
            self.assertEqual(self.test_cam.bin_y, 1)  # Defaults to 1
        except AttributeError:
            self.skipTest("test_get_bin_y: This camera does not "
                          "support binning.")


    def test_get_bin_y_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.bin_y

    def test_get_chip_name(self):
        self.test_cam.open()
        chip_name = pvc.get_param(self.test_cam.handle,
                                  const.PARAM_CHIP_NAME,
                                  const.ATTR_CURRENT)
        self.assertEqual(chip_name, self.test_cam.chip_name)

    def test_get_chip_name_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.chip_name

    def test_get_serial_no(self):
        self.test_cam.open()
        ser_no = pvc.get_param(self.test_cam.handle,
                               const.PARAM_HEAD_SER_NUM_ALPHA,
                               const.ATTR_CURRENT)
        self.assertEqual(ser_no, self.test_cam.serial_no)

    def test_get_serial_no_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.serial_no

    def test_get_speed_table_index(self):
        self.test_cam.open()
        spdtab_index = pvc.get_param(self.test_cam.handle,
                                     const.PARAM_SPDTAB_INDEX,
                                     const.ATTR_CURRENT)
        self.assertEqual(spdtab_index, self.test_cam.speed_table_index)

    def test_get_speed_table_index_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.speed_table_index

    def test_set_speed_table_index(self):
        self.test_cam.open()
        num_entries = self.test_cam.get_param(const.PARAM_SPDTAB_INDEX,
                                              const.ATTR_COUNT)
        for i in range(num_entries):
            self.test_cam.speed_table_index = i
            self.assertEqual(i, self.test_cam.speed_table_index)

    def test_set_speed_table_index_out_of_bounds(self):
        self.test_cam.open()
        num_entries = self.test_cam.get_param(const.PARAM_SPDTAB_INDEX,
                                              const.ATTR_COUNT)
        with self.assertRaises(ValueError):
            self.test_cam.speed_table_index = num_entries

    def test_set_speed_table_index_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.speed_table_index = 0
        
    def test_get_readout_port(self):
        self.test_cam.open()
        readout_port = pvc.get_param(self.test_cam.handle, const.PARAM_READOUT_PORT,
                                     const.ATTR_CURRENT)
        self.assertEqual(readout_port, self.test_cam.readout_port)

    def test_get_readout_port_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.readout_port

    def test_set_readout_port_index(self):
        self.test_cam.open()
        num_entries = self.test_cam.get_param(const.PARAM_READOUT_PORT,
                                              const.ATTR_COUNT)
        for i in range(num_entries):
            self.test_cam.readout_port_index = i
            self.assertEqual(i, self.test_cam.readout_port)

    def test_set_readout_port_out_of_bounds(self):
        self.test_cam.open()
        num_entries = self.test_cam.get_param(const.PARAM_READOUT_PORT,
                                         const.ATTR_COUNT)
        with self.assertRaises(ValueError):
            self.test_cam.readout_port = num_entries

    def test_set_readout_port_no_open_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.readout_port = 0

    def test_get_bit_depth(self):
        self.test_cam.open()
        curr_bit_depth = pvc.get_param(self.test_cam.handle, 
                                       const.PARAM_BIT_DEPTH,
                                       const.ATTR_CURRENT)
        self.assertEqual(curr_bit_depth, self.test_cam.bit_depth)

    def test_get_bit_depth_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.bit_depth

    def test_get_pix_time(self):
        self.test_cam.open()
        curr_pix_time = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_PIX_TIME,
                                      const.ATTR_CURRENT)
        self.assertEqual(curr_pix_time, self.test_cam.pix_time)

    def test_get_pix_time_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.pix_time

    def test_get_gain(self):
        self.test_cam.open()
        curr_gain_index = pvc.get_param(self.test_cam.handle,
                                        const.PARAM_GAIN_INDEX,
                                        const.ATTR_CURRENT)
        self.assertEqual(curr_gain_index, self.test_cam.gain)

    def test_get_gain_fail(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.gain

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

    def test_set_gain_less_than_zero(self):
        self.test_cam.open()
        with self.assertRaises(ValueError):
            self.test_cam.gain = -1

    def test_set_gain_more_than_max(self):
        self.test_cam.open()
        max_gain = pvc.get_param(self.test_cam.handle,
                                 const.PARAM_GAIN_INDEX,
                                 const.ATTR_MAX)
        with self.assertRaises(ValueError):
            self.test_cam.gain = max_gain + 1

    def test_set_gain_no_open(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.gain = 1 

    def test_adc_offset(self):
        self.test_cam.open()
        curr_adc_offset = pvc.get_param(self.test_cam.handle,
                                        const.PARAM_ADC_OFFSET,
                                        const.ATTR_CURRENT)
        self.assertEqual(curr_adc_offset, self.test_cam.adc_offset)

    def test_adc_offset_no_open(self):
        # self.assertRaises(RuntimeError, getattr, self.test_cam, "adc_offset")
        with self.assertRaises(RuntimeError):
            self.test_cam.adc_offset

    def test_get_clear_mode(self):
        self.test_cam.open()
        curr_clear_mode = pvc.get_param(self.test_cam.handle,
                                         const.PARAM_CLEAR_MODE,
                                         const.ATTR_CURRENT)
        self.assertEqual(curr_clear_mode, self.test_cam.clear_mode)
        # reversed_dict = {val: key for key, val in const.clear_modes.items()}
        # self.assertEqual(reversed_dict[curr_clear_mode],
        #                  self.test_cam.clear_mode)

    def test_get_clear_mode_no_open(self):
        with self.assertRaises(RuntimeError):
            self.test_cam.clear_mode

    def test_set_clear_mode_by_name(self):
        self.test_cam.open()
        modes = set(self.test_cam.read_enum(const.PARAM_CLEAR_MODE))
        for mode in set(const.clear_modes).intersection(modes):
            self.test_cam.clear_mode = mode
            curr_clear_mode = pvc.get_param(self.test_cam.handle,
                                            const.PARAM_CLEAR_MODE,
                                            const.ATTR_CURRENT)
            self.assertEqual(curr_clear_mode, self.test_cam.clear_mode)

    def test_set_clear_mode_by_value(self):
        self.test_cam.open()
        modes = set(self.test_cam.read_enum(const.PARAM_CLEAR_MODE).values())
        for mode in set(const.clear_modes.values()).intersection(modes):
            self.test_cam.clear_mode = mode
            curr_clear_mode = pvc.get_param(self.test_cam.handle,
                                            const.PARAM_CLEAR_MODE,
                                            const.ATTR_CURRENT)
            self.assertEqual(curr_clear_mode, self.test_cam.clear_mode)

    def test_set_clear_mode_bad_name_fail(self):
        self.test_cam.open()
        self.assertRaises(ValueError, setattr, self.test_cam,
                          "clear_mode", "")

    def test_set_clear_mode_bad_value_fail(self):
        self.test_cam.open()
        self.assertRaises(ValueError, setattr, self.test_cam,
                          "clear_mode", -1)

    def test_set_clear_mode_no_open_fail(self):
        self.assertRaises(RuntimeError, setattr, self.test_cam,
                          "clear_mode", 0)

    # TODO: All setters should raise Runtime if not open
    def test_set_clear_mode_no_open_bad_value_fail(self):
        self.assertRaises(ValueError, setattr, self.test_cam,
                          "clear_mode", -1)

    def test_get_temp(self):
        self.test_cam.open()
        curr_temp = pvc.get_param(self.test_cam.handle, const.PARAM_TEMP,
                                  const.ATTR_CURRENT)
        # Less than +/-2% variation of temperature between calls.
        self.assertGreaterEqual(abs(curr_temp / self.test_cam.temp), 0.98)

    def test_get_temp_no_open_fail(self):
        self.assertRaises(RuntimeError, getattr, self.test_cam, "temp")

    def test_get_temp_setpoint(self):
        self.test_cam.open()
        curr_temp_setpoint = pvc.get_param(self.test_cam.handle,
                                           const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_CURRENT)
        self.assertEqual(curr_temp_setpoint, self.test_cam.temp_setpoint)

    def test_get_temp_setpoint_no_open_fail(self):
        self.assertRaises(RuntimeError, getattr, self.test_cam, "temp_setpoint")

    def test_set_temp_setpoint(self):
        self.test_cam.open()
        min_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_MIN)
        max_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                           const.ATTR_MAX)
        for i in range(max_temp, min_temp-1, -1):
            self.test_cam.temp_setpoint = i
            self.assertEqual(i, self.test_cam.temp_setpoint)

    def test_set_temp_to_high_fail(self):
        self.test_cam.open()
        one_over_max_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                                    const.ATTR_MAX) + 1
        self.assertRaises(ValueError, setattr, self.test_cam,
                          "temp_setpoint", one_over_max_temp)

    def test_set_temp_to_low_fail(self):
        self.test_cam.open()
        one_below_max_temp = self.test_cam.get_param(const.PARAM_TEMP_SETPOINT,
                                                     const.ATTR_MIN) - 1
        self.assertRaises(ValueError, setattr, self.test_cam,
                          "temp_setpoint", one_below_max_temp)

    def test_set_temp_no_open_fail(self):
        self.assertRaises(RuntimeError, setattr, self.test_cam,
                          "temp_setpoint", 0)

    def test_get_exp_res(self):
        self.test_cam.open()
        curr_exp_res_index = pvc.get_param(self.test_cam.handle,
                                           const.PARAM_EXP_RES_INDEX,
                                           const.ATTR_CURRENT)
        self.assertEqual(curr_exp_res_index, self.test_cam.exp_res_index)

    def test_set_exp_res_by_name(self):
        self.test_cam.open()
        for i in range(self.test_cam.exp_res_index):
            self.test_cam.exp_res = const.exp_resolutions[i]
            curr_exp_res = pvc.get_param(self.test_cam.handle,
                                         const.PARAM_EXP_RES,
                                         const.ATTR_CURRENT)
            self.assertEqual(curr_exp_res, self.test_cam.exp_res)

    def test_set_exp_res_bad_name_fail(self):
        self.test_cam.open()
        self.assertRaises(ValueError, setattr, self.test_cam,
                          "exp_res", "")

    def test_set_exp_res_bad_value_fail(self):
        self.test_cam.open()
        self.assertRaises(ValueError, setattr, self.test_cam,
                          "exp_res", -1)

    def test_set_exp_res_no_open_fail(self):
        self.assertRaises(RuntimeError, setattr, self.test_cam,
                          "exp_res", 0)

    def test_get_sensor_size(self):
        self.test_cam.open()
        rows = pvc.get_param(self.test_cam.handle, const.PARAM_SER_SIZE,
                             const.ATTR_CURRENT)
        cols = pvc.get_param(self.test_cam.handle, const.PARAM_PAR_SIZE,
                             const.ATTR_CURRENT)
        self.assertEqual((rows, cols), self.test_cam.sensor_size)

    def test_get_sensor_no_open_fail(self):
        self.assertRaises(RuntimeError, getattr, self.test_cam, "sensor_size")

    def test_get_exp_mode(self):
        self.test_cam.open()
        curr_exp_mode = pvc.get_param(self.test_cam.handle,
                                      const.PARAM_EXPOSURE_MODE,
                                      const.ATTR_CURRENT)
        self.assertEqual(curr_exp_mode, self.test_cam.exp_mode)

    def test_get_exp_mode_no_open(self):
        self.assertRaises(RuntimeError, getattr, self.test_cam, "exp_mode")

def main():
    unittest.main()

if __name__ == '__main__':
    main()
