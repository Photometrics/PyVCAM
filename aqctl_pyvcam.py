#!/usr/bin/env python3
"""
aqctl_pyvcam.py

Implement ARTIQ Network Device Support Package (NDSP)
to support Teledyne ion camera integration into ARTIQ experiment.

Kevin Chen
2023-02-24
University of Waterloo
QuantumIon
"""

from sipyco.pc_rpc import simple_server_loop
from sipyco import common_args
import argparse
import logging
from pyvcam import pvc
from pyvcam.camera import Camera

logger = logging.getLogger(__name__)

class PyVCAM:
    """
    Teledyne PrimeBSI ion camera control.
    """
    def __init__(self) -> None:
        """
        Creates camera object.
        NOTE: This does not open the camera. User has to call open function.
        """
        pvc.init_pvcam()
        self.cam = [cam for cam in Camera.detect_camera()][0]

    def __del__(self) -> None:
        """
        Closes and destroys camera object during garbage collection.
        NOTE: close() function returns an error if user has already called close.
        """
        try:
            self.cam.close()
        finally:
            pvc.uninit_pvcam()

    def detect_camera(self) -> NotImplementedError:
        """
        Function already called in __init__(). User should not explicitly call this function.

        :raises NotImplementedError:
        """
        raise NotImplementedError("""Function should not be explicitly called; 
                                    already implemented during initialization.""")

    def open(self) -> None:
        """
        Opens camera.

        :return: None.
        """
        self.cam.open()

    def close(self) -> None:
        """
        Closes camera. Automatically called during uninitialization.

        :return: None.
        """
        self.cam.close()
    
    def get_frame(self, exp_time: int=None, timeout_ms: int=-1) -> list[list[int]]:
        """
        Calls the pvc.get_frame function with the current camera settings.

        :param exp_time: Exposure time (optional).
        :param timeout_ms: Timeout time (optional). Default set to no timeout (forever).
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
    
    def get_param(self, param_id: int, param_attr: int) -> int:
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
        Returns bit depth property.

        :return:
        """
        return self.cam.bit_depth

    def cam_fw(self) -> str:
        """
        Returns camera firmware version.

        :return:
        """
        return self.cam.cam_fw

    def driver_version(self) -> str:
        """
        Returns camera driver version.

        :return:
        """
        return self.cam.driver_version

    def exp_mode(self) -> str:
        """
        Returns exposure mode property.
        self.cam.exp_mode returns int, but we want to return string
        within dict returned by exp_modes().

        :return:
        """
        return list(self.exp_modes().keys())[list(self.exp_modes().values()).index(self.cam.exp_mode)]

    def set_exp_mode(self, keyOrValue: int) -> None:
        """
        Changes exposure mode.
        Will raise ValueError if provided with an unrecognized key.
        Keys:
        1792 - Internal Trigger
        2304 - Edge Trigger
        2048 - Trigger first

        :param keyOrValue: Key or value to change.
        :return: None.
        """
        self.cam.exp_mode = keyOrValue

    def exp_modes(self) -> dict[str, int]:
        """
        Returns dict of available exposure modes to select.

        :return:
        """
        return dict(self.cam.exp_modes)

    def exp_res(self) -> str:
        """
        Returns exposure resolution property.
        self.cam.exp_res returns int, but we want to return string
        within dict returned by exp_resolutions().

        :return:
        """
        return list(self.exp_resolutions().keys())[list(self.exp_resolutions().values()).index(self.cam.exp_res)]

    def set_exp_res(self, keyOrValue: int) -> None:
        """
        Changes exposure resolution.
        Keys:
        0 - One Millisecond
        1 - One Microsecond

        :param keyOrValue: Key or value to change.
        :return: None.
        """
        self.cam.exp_res = keyOrValue

    def exp_res_index(self) -> int:
        """
        Returns exposure resolution index.

        :return:
        """
        return self.cam.exp_res_index

    def exp_resolutions(self) -> dict[str, int]:
        """
        Returns dict of available resolutions to select.

        :return:
        """
        return dict(self.cam.exp_resolutions)

    def exp_time(self) -> int:
        """
        Returns exposure time property.

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
        Returns gain property.

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
        Returns last exposure time.

        :return:
        """
        return self.cam.last_exp_time

    def pix_time(self) -> int:
        """
        Returns pixel time.

        :return:
        """
        return self.cam.pix_time

    def port_speed_gain_table(self) -> dict:
        """
        Returns a dict of keys and dicts resembling a table.

        :return:
        """
        return self.cam.port_speed_gain_table

    def readout_port(self) -> int:
        """
        Returns readout port property. By default only one available value (0).

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
        Returns serial number of camera.

        :return:
        """
        return self.cam.serial_no

def get_argparser():
    parser = argparse.ArgumentParser(description="""PyVCAM controller.
    Use this controller to drive the Teledyne ion camera.""")
    common_args.simple_network_args(parser, 3249)
    parser.add_argument("--someargument", default=None, help="PyVCAM Wrapper. See documentation at https://github.com/Photometrics/PyVCAM")
    common_args.verbosity_args(parser)
    return parser

def main():
    args = get_argparser().parse_args()
    common_args.init_logger_from_args(args)
    logger.info("PyVCAM open.")
    simple_server_loop({"pyvcam": PyVCAM()}, "::1", 3249)

if __name__ == "__main__":
    main()