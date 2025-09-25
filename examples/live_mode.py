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
    bytes_per_pixel = int((bit_depth + 7) / 8)
    max_pix_value = 2 ** (bytes_per_pixel * 8) - 1

    cam.start_live(exp_time=5)

    width = 800
    height = int(cam.sensor_size[1] * width / cam.sensor_size[0])
    dim = (width, height)

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

        # Shrink to smaller window
        disp_img = cv2.resize(frame['pixel_data'], dim,
                              interpolation=cv2.INTER_AREA)
        # Normalize to min-max range to make even background noise visible
        disp_img = cv2.normalize(disp_img, dst=None,
                                 alpha=0, beta=max_pix_value,
                                 norm_type=cv2.NORM_MINMAX)
        cv2.imshow('Live Mode', disp_img)

        if cv2.waitKey(10) == 27:
            break

    t_end = time.time()
    fps_avg = cnt / (t_end - t_start)
    print(f'\nTotal frames: {cnt}, dropped: {dropped}, average fps: {fps_avg:.1f}')

    cam.finish()

    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
