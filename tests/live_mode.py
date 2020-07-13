from pyvcam import pvc
from pyvcam.camera import Camera
import time
import cv2
import numpy as np


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    cam.start_live(exp_time=20)
    cnt = 0
    tot = 0
    t1 = time.time()
    start = time.time()
    width = 800
    height = int(cam.sensor_size[1] * width / cam.sensor_size[0])
    dim = (width, height)
    fps = 0

    while True:
        frame = cam.get_live_frame().reshape(cam.sensor_size[::-1])
        frame = cv2.resize(frame,dim, interpolation = cv2.INTER_AREA)
        cv2.imshow('Live Mode', frame)

        low = np.amin(frame)
        high = np.amax(frame)
        average = np.average(frame)

        if cnt == 10:
                t1 = time.time() - t1
                fps = 10/t1
                t1 = time.time()
                cnt = 0
        if cv2.waitKey(10) == 27:
            break
        print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Rate: {:.1f}\n'.format(low, high, average, fps))
        cnt += 1
        tot += 1
    
    cam.close()
    pvc.uninit_pvcam()

    print('Total frames: {}\nAverage fps: {}\n'.format(tot, (tot/(time.time()-start))))
if __name__ == "__main__":
    main()
