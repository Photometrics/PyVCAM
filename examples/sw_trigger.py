import threading
import time

from pyvcam import pvc
from pyvcam.camera import Camera

NUM_FRAMES = 20


class TriggerThreadRun(threading.Thread):
    def __init__(self, cam):
        threading.Thread.__init__(self)
        self.cam = cam
        self.end = False

    def run(self):
        while not self.end:
            try:
                self.cam.sw_trigger()
                print('SW Trigger success')
                time.sleep(0.05)
            except ValueError as e:
                print(str(e))

    def stop(self):
        self.end = True


def collect_frames(cam, num_frames):
    frames_received = 0
    while frames_received < num_frames:
        try:
            frame, fps, frame_count = cam.poll_frame()
            print(f"Count: {frame_count:2}  FPS: {fps:5.1f}"
                  f"  First five pixels: {frame['pixel_data'][0, 0:5]}")
            frames_received += 1
        except ValueError as e:
            print(str(e))

    return frames_received


def main():
    # Initialize PVCAM and find the first available camera.
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    cam.exp_mode = 'Software Trigger Edge'

    # Start a thread for executing the trigger
    t1 = TriggerThreadRun(cam)
    t1.start()

    # Collect frames in live mode
    cam.start_live()
    frames_received = collect_frames(cam, NUM_FRAMES)
    cam.finish()
    print(f'Received live frames: {frames_received}\n')

    # Collect frames in sequence mode
    cam.start_seq(num_frames=NUM_FRAMES)
    frames_received = collect_frames(cam, NUM_FRAMES)
    cam.finish()
    print(f'Received sequence frames: {frames_received}\n')

    t1.stop()

    cam.close()
    pvc.uninit_pvcam()

    print('Done')


if __name__ == "__main__":
    main()
