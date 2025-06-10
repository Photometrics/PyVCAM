import time

import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera

NUM_FRAMES = 5
EXPOSE_TIME_MS = 200


def print_frame_details(frame, frame_count, desc):
    low = np.amin(frame['pixel_data'])
    high = np.amax(frame['pixel_data'])
    avg = np.average(frame['pixel_data'])
    print(f'{desc}:  Min: {low}\tMax: {high}\tAverage: {avg:.0f}'
          f'\tFrames: {frame_count}')


def main():
    # Initialize PVCAM and find the first available camera.
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    cam.start_seq(exp_time=EXPOSE_TIME_MS, num_frames=NUM_FRAMES)

    # Wait for 3 frames to be captured.
    # Frames take a slightly longer than expose time, so wait a little longer.
    time.sleep(3 * EXPOSE_TIME_MS * 1e-3 + 0.1)

    cnt = 0
    while cnt < NUM_FRAMES:
        frame, _, frame_count = cam.poll_frame(oldestFrame=False)
        print_frame_details(frame, frame_count, 'Newest')

        frame, _, frame_count = cam.poll_frame(oldestFrame=True)
        print_frame_details(frame, frame_count, 'Oldest')

        cnt += 1

    cam.finish()

    cam.close()
    pvc.uninit_pvcam()

    print(f'\nTotal frames: {cnt}')


if __name__ == "__main__":
    main()
