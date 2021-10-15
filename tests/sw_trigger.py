import threading
import time

from pyvcam import pvc
from pyvcam.camera import Camera

NUM_FRAMES = 20

class TriggerThreadRun (threading.Thread):
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
            except Exception as e:
                pass

    def stop(self):
        self.end = True

def collectFrames(cam):
    framesReceived = 0

    while framesReceived < NUM_FRAMES:
        time.sleep(0.005)

        try:
            frame, fps, frame_count = cam.poll_frame()
            print('Count: {} FPS: {} First five pixels of frame: {}'.format(frame_count, round(fps, 2), frame['pixel_data'][0, 0:5]))
            framesReceived += 1
        except Exception as e:
            print(str(e))

    return framesReceived

def main():
    # Initialize PVCAM and find the first available camera.
    pvc.init_pvcam()

    cam = [cam for cam in Camera.detect_camera()][0]
    cam.open()
    cam.speed_table_index = 0
    cam.exp_mode = 'Software Trigger Edge'

    # Start a thread for executing the trigger
    t1 = TriggerThreadRun(cam)
    t1.start()

    # Collect frames in live mode
    cam.start_live()
    framesReceived = collectFrames(cam)
    cam.finish()
    print('Received live frames: ' + str(framesReceived) + '\n')

    # Collect frames in sequence mode
    cam.start_seq(num_frames=NUM_FRAMES)
    framesReceived = collectFrames(cam)
    cam.finish()
    print('Received seq frames: ' + str(framesReceived) + '\n')

    t1.stop()
    cam.close()
    pvc.uninit_pvcam()

    print('Done')

if __name__ == "__main__":
    main()
