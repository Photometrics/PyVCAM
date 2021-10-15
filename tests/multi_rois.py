import time
import cv2
import matplotlib.pyplot as plt
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam.constants import rgn_type

def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()

    cam.meta_data_enabled = True

    cam.set_roi(0, 0, 250, 250)
    cam.set_roi(300, 100, 400, 900)
    cam.set_roi(750, 300, 200, 200)

    NUM_FRAMES = 1

    cnt = 0

    cam.start_seq(exp_time=20, num_frames=NUM_FRAMES)
    while cnt < NUM_FRAMES:
        frame, fps, frame_count = cam.poll_frame()

        num_rois = len(frame['pixel_data'])
        for roi_index in range(num_rois):
            low = np.amin(frame['pixel_data'][roi_index])
            high = np.amax(frame['pixel_data'][roi_index])
            average = np.average(frame['pixel_data'][roi_index])

            s1 = frame['meta_data']['roi_headers'][roi_index]['roi']['s1']
            s2 = frame['meta_data']['roi_headers'][roi_index]['roi']['s2']
            p1 = frame['meta_data']['roi_headers'][roi_index]['roi']['p1']
            p2 = frame['meta_data']['roi_headers'][roi_index]['roi']['p2']
            print('ROI:{:2} ({:4}, {:4}) ({:4}, {:4})\tMin:{}\tMax:{}\tAverage:{:.0f}\tFrame Rate: {:.1f}\tFrame Count: {:.0f}'.format(roi_index, s1, p1, s2, p2, low, high, average, fps, frame_count))

            alpha = 2.0  # Contrast control (1.0-3.0)
            beta = 10  # Brightness control (0-100)
            img = cv2.convertScaleAbs(frame['pixel_data'][roi_index], alpha=alpha, beta=beta)

            window_name = 'Region: {}'.format(roi_index)
            cv2.imshow(window_name, img)
            cv2.moveWindow(window_name, s1, p1)

        cnt += 1

        cv2.waitKey(0)

    cam.finish()
    cam.close()
    pvc.uninit_pvcam()

    print('Total frames: {}\n'.format(cnt))


if __name__ == "__main__":
    main()
