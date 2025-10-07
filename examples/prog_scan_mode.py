import time
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const

NUM_FRAMES = 10
EXP_TIME = 10
SCAN_LINE_DELAY = 10
SCAN_WIDTH = 10


def capture_stack_and_print_details(cam):
    print(f'Scan line time: {cam.prog_scan_line_time} ns')
    print(f'Scan width: {cam.prog_scan_width} lines')

    start = time.time()
    stack = cam.get_sequence(num_frames=NUM_FRAMES, exp_time=EXP_TIME)
    end = time.time()

    # Numpy's mean can be inaccurate in single precision
    mean = np.mean(stack, dtype=np.float64)

    print(f'First five pixels of first frame: {stack[0][0][:5]}')
    print(f'Mean: {mean:.3f} ADU')
    print(f'Time to capture sequence: {end - start:.6f} s')


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    if not cam.check_param(const.PARAM_SCAN_MODE):
        print('Camera does not support Programmable Scan Mode feature')
        return

    # cam.readout_port = 'Dynamic Range'
    # cam.speed = 0
    # cam.gain = 1

    # Run one acq. to preallocate all buffers to not distort measurements below
    _ = cam.get_sequence(num_frames=NUM_FRAMES, exp_time=0)

    ##########

    print('\nSetting PSM to Auto')
    cam.prog_scan_mode = const.PL_SCAN_MODE_AUTO

    capture_stack_and_print_details(cam)

    ##########

    print('\nSetting PSM to Line Delay')
    cam.prog_scan_mode = const.PL_SCAN_MODE_PROGRAMMABLE_LINE_DELAY

    print(f'Setting scan direction to UP and line delay to {SCAN_LINE_DELAY}')
    cam.prog_scan_dir = const.PL_SCAN_DIRECTION_UP
    cam.prog_scan_line_delay = SCAN_LINE_DELAY

    capture_stack_and_print_details(cam)

    ##########

    print('\nSetting PSM to Scan Width')
    cam.prog_scan_mode = const.PL_SCAN_MODE_PROGRAMMABLE_SCAN_WIDTH

    print(f'Setting scan direction to UP and scan width to {SCAN_WIDTH}')
    cam.prog_scan_dir = const.PL_SCAN_DIRECTION_UP
    cam.prog_scan_width = SCAN_WIDTH

    capture_stack_and_print_details(cam)

    ##########

    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
