import time

import cv2
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    has_metadata = cam.check_param(const.PARAM_METADATA_ENABLED)
    if has_metadata:
        cam.set_param(const.PARAM_METADATA_ENABLED, True)

    # The 'bit_depth_host' provides a bit-depth of the pixels returned by PVCAM.
    # The 'bit_depth' is a bit-depth from the sensor. In most cases both values
    # are same. But for example when a frame summing feature is active, the
    # sensors still reads 12-bit pixels internally, while PVCAM returns 16-bit
    # or even 32-bit pixels to the application.
    # Ideally, max_pix_value should be based on PARAM_IMAGE_FORMAT_HOST.
    bit_depth = cam.bit_depth_host
    bytes_per_pixel = (bit_depth + 7) // 8
    max_pix_value = 2 ** (bytes_per_pixel * 8) - 1

    # For color camera prepare OpenCV code to de-bayer to its native BGR format
    cvt_code = None
    if cam.check_param(const.PARAM_COLOR_MODE):
        color_mode = cam.get_param(const.PARAM_COLOR_MODE)
        if color_mode == const.COLOR_RGGB:
            cvt_code = cv2.COLOR_BayerRGGB2BGR
        if color_mode == const.COLOR_GRBG:
            cvt_code = cv2.COLOR_BayerGRBG2BGR
        if color_mode == const.COLOR_GBRG:
            cvt_code = cv2.COLOR_BayerGBRG2BGR
        if color_mode == const.COLOR_BGGR:
            cvt_code = cv2.COLOR_BayerBGGR2BGR

    # Calculate shrunken display window size that keeps ratio with sensor
    win_width = 800
    win_height = (cam.sensor_size[1] * win_width) // cam.sensor_size[0]
    win_dim = (win_width, win_height)

    # Create the window in advance
    win_name = 'Live Mode'
    cv2.namedWindow(win_name, cv2.WINDOW_AUTOSIZE)  # Non-resizable by user
    cv2.resizeWindow(win_name, win_dim)
    cv2.setWindowTitle(win_name, f'{win_name} - <no image yet>')

    cam.start_live(exp_time=5)

    cnt = 0
    dropped = 0
    t_start = time.time()

    while True:
        frame, fps, frame_count = cam.poll_frame()
        cnt += 1
        if cnt < frame_count:
            if cnt + 1 == frame_count:
                print(f'Dropped frame {cnt}')
            else:
                print(f'Dropped frames {cnt}-{frame_count - 1}')
            dropped += frame_count - cnt
            cnt = frame_count
        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        avg = np.average(frame['pixel_data'])
        meta_str = f'\tNr.: {frame["meta_data"]["frame_header"]["frameNr"]}' \
            if has_metadata else ''
        print(f'Min: {low}\tMax: {high}\tAverage: {avg:.0f}'
              f'\tFrames: {frame_count}\tFrame Rate: {fps:.1f}'
              f'{meta_str}')

        disp_img = frame['pixel_data']
        # De-bayer the original image, if it is color
        if cvt_code is not None:
            disp_img = cv2.cvtColor(disp_img, cvt_code)
        # Shrink to smaller window
        disp_img = cv2.resize(disp_img, win_dim,
                              interpolation=cv2.INTER_AREA)
        # Normalize to min-max range to make even background noise visible
        disp_img = cv2.normalize(disp_img, dst=None,
                                 alpha=0, beta=max_pix_value,
                                 norm_type=cv2.NORM_MINMAX)

        cv2.imshow(win_name, disp_img)
        cv2.setWindowTitle(win_name, f'{win_name} - #{frame_count}, {fps:.1f} FPS')

        if cv2.waitKey(10) == 27:  # Exit on <ESC> key
            break

    t_end = time.time()
    fps_avg = cnt / (t_end - t_start)
    print(f'\nTotal frames: {cnt}, dropped: {dropped}, average fps: {fps_avg:.1f}')

    cam.finish()

    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
