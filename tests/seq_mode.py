import time
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants

def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print('Camera: {}'.format(cam.name))

    NUM_FRAMES = 10
    cnt = 0

    print('\nUsing start_seq+poll_frame:')
    cam.start_seq(exp_time=20, num_frames=NUM_FRAMES)
    while cnt < NUM_FRAMES:
        frame, fps, frame_count = cam.poll_frame()
        cnt += 1
        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        average = np.average(frame['pixel_data'])
        print('Min: {}\tMax: {}\tAverage: {:.0f}\tFrames: {:.0f}\tFrame Rate: {:.1f}'
            .format(low, high, average, frame_count, fps))
        time.sleep(0.05)
    cam.finish()

    print('\nUsing get_sequence:')
    frames = cam.get_sequence(NUM_FRAMES)
    for frame in frames:
        cnt += 1
        low = np.amin(frame)
        high = np.amax(frame)
        average = np.average(frame)
        print('Min: {}\tMax: {}\tAverage: {:.0f}\tFrames: {:.0f}'
            .format(low, high, average, cnt))

    print('\nUsing get_vtm_sequence:')
    time_list = [i*10 for i in range(1, NUM_FRAMES+1)]
    frames = cam.get_vtm_sequence(time_list, constants.EXP_RES_ONE_MILLISEC, NUM_FRAMES)
    i = 0
    for frame in frames:
        cnt += 1
        low = np.amin(frame)
        high = np.amax(frame)
        average = np.average(frame)
        print('Min: {}\tMax: {}\tAverage: {:.0f}\tFrames: {:.0f}\tExp. time: {}'
            .format(low, high, average, cnt, time_list[i]))
        i += 1

    cam.close()
    pvc.uninit_pvcam()

    print('\nTotal frames: {}'.format(cnt))


if __name__ == "__main__":
    main()
