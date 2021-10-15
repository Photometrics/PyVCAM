import numpy as np
import threading
import time

from pyvcam import pvc
from pyvcam.camera import Camera

NUM_FRAMES = 5
EXPOSE_TIME_ms = 200

def printFrameDetails(frame, fps, frame_count, desc):
    low = np.amin(frame['pixel_data'])
    high = np.amax(frame['pixel_data'])
    average = np.average(frame['pixel_data'])
    print(desc + ': Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Count: {:.0f}'.format(low, high, average, frame_count))

def main():
    # Initialize PVCAM and find the first available camera.
    pvc.init_pvcam()

    cam = [cam for cam in Camera.detect_camera()][0]
    cam.open()

    cam.start_seq(exp_time=EXPOSE_TIME_ms, num_frames=NUM_FRAMES)

    # Wait for 3 frames to be captured. Frames take a slightly longer than expose time, so wait a little extra time
    time.sleep(3 * EXPOSE_TIME_ms * 1e-3 + 0.1)

    cnt = 0
    while cnt < NUM_FRAMES:
        frame, fps, frame_count = cam.poll_frame(oldestFrame=False)
        printFrameDetails(frame, fps, frame_count, 'newest')

        frame, fps, frame_count = cam.poll_frame()
        printFrameDetails(frame, fps, frame_count, 'oldest')

        cnt += 1

    cam.finish()
    cam.close()
    pvc.uninit_pvcam()

    print('\nTotal frames: {}'.format(cnt))

if __name__ == "__main__":
    main()
