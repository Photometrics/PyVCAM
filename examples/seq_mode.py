import time
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const

NUM_FRAMES = 10


def print_frame_details(frame, frame_count, extra_info=''):
    low = np.amin(frame)
    high = np.amax(frame)
    avg = np.average(frame)
    print(f'Min: {low}\tMax: {high}\tAverage: {avg:.0f}'
          f'\tFrames: {frame_count}{extra_info}')


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    cnt = 0

    print('\nUsing start_seq+poll_frame+finish:')
    cam.start_seq(exp_time=20, num_frames=NUM_FRAMES)
    while cnt < NUM_FRAMES:
        frame, fps, frame_count = cam.poll_frame()
        cnt += 1
        print_frame_details(frame['pixel_data'], frame_count,
                            f'\tFrame Rate: {fps:.1f}')
        time.sleep(0.05)
    cam.finish()

    print('\nUsing get_sequence:')
    frames = cam.get_sequence(NUM_FRAMES)
    for frame in frames:
        cnt += 1
        print_frame_details(frame, cnt)

    print('\nUsing get_vtm_sequence:')
    time_list = [i * 10 for i in range(1, 8)]
    frames = cam.get_vtm_sequence(time_list, const.EXP_RES_ONE_MILLISEC, NUM_FRAMES)
    i = 0
    for frame in frames:
        cnt += 1
        print_frame_details(frame, cnt, f'\tExp. time: {time_list[i]}')
        i = (i + 1) % len(time_list)

    try:
        print('\nUsing poll_frame without starting acquisition:')
        frame, fps, frame_count = cam.poll_frame()
        cnt += 1
        print_frame_details(frame['pixel_data'], frame_count,
                            f'\tFrame Rate: {fps:.1f}')
    except RuntimeError as e:
        print(f'Expected error: {e}')

    print('\nUsing get_frame alone:')
    frame = cam.get_frame()
    cnt += 1
    print_frame_details(frame, cnt)

    cam.close()
    pvc.uninit_pvcam()

    print(f'\nTotal frames: {cnt}')


if __name__ == "__main__":
    main()
