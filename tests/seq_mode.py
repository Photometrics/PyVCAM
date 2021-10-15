import time
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants

def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()

    NUM_FRAMES = 10
    cnt = 0

    cam.start_seq(exp_time=20, num_frames=NUM_FRAMES)
    while cnt < NUM_FRAMES:
        frame, fps, frame_count = cam.poll_frame()

        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        average = np.average(frame['pixel_data'])

        print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Rate: {:.1f}\tFrame Count: {:.0f}\n'.format(low, high, average, fps, frame_count))
        cnt += 1

        time.sleep(0.05)

    cam.finish()

    # Test basic sequence methods
    frames = cam.get_sequence(NUM_FRAMES)
    for frame in frames:
        low = np.amin(frame)
        high = np.amax(frame)
        average = np.average(frame)

        print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Count: {:.0f}\n'.format(low, high, average, cnt))
        cnt += 1

    time_list = [i*10 for i in range(1, NUM_FRAMES+1)]
    frames = cam.get_vtm_sequence(time_list, constants.EXP_RES_ONE_MILLISEC, NUM_FRAMES)
    for frame in frames:
        low = np.amin(frame)
        high = np.amax(frame)
        average = np.average(frame)

        print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Count: {:.0f}\n'.format(low, high, average, cnt))
        cnt += 1

    cam.close()
    pvc.uninit_pvcam()

    print('Total frames: {}\n'.format(cnt))


if __name__ == "__main__":
    main()
