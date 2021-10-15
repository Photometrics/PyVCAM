import time
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants

def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    cam.meta_data_enabled = True
    cam.set_roi = (0, 0, 1000, 1000)

    num_frames = 1
    cnt = 0

    cam.start_seq(exp_time=20, num_frames=num_frames)
    while cnt < num_frames:
        frame, fps, frame_count = cam.poll_frame()

        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        average = np.average(frame['pixel_data'])

        print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Rate: {:.1f}\tFrame Count: {:.0f}\n'.format(low, high, average, fps, frame_count))
        cnt += 1

        time.sleep(0.05)

    cam.finish()
    cam.close()
    pvc.uninit_pvcam()

    print('Total frames: {}\n'.format(cnt))


if __name__ == "__main__":
    main()
