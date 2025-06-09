import time

import cv2
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    cam.start_live(exp_time=20)

    width = 800
    height = int(cam.sensor_size[1] * width / cam.sensor_size[0])
    dim = (width, height)

    cnt = 0
    t_start = time.time()

    while True:
        frame, fps, frame_count = cam.poll_frame()
        cnt += 1

        frame['pixel_data'] = cv2.resize(frame['pixel_data'], dim,
                                         interpolation=cv2.INTER_AREA)
        cv2.imshow('Live Mode', frame['pixel_data'])

        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        avg = np.average(frame['pixel_data'])

        if cv2.waitKey(10) == 27:
            break

        print(f'Min: {low}\tMax: {high}\tAverage: {avg:.0f}'
              f'\tFrames: {frame_count}\tFrame Rate: {fps:.1f}')

    t_end = time.time()
    fps_avg = cnt / (t_end - t_start)
    print(f'\nTotal frames: {cnt}, Average fps: {fps_avg:.1f}')

    cam.abort()

    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
