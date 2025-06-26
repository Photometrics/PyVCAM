import cv2
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const

NUM_FRAMES = 1


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    # When changing anything in speed table it is strongly recommended to set
    # all 3 properties (readout_port, speed, gain) in predefined order.
    # After changing `readout_port` re-apply the value of `speed`.
    # After changing `speed` re-apply the `gain` value.

    # Kinetix supports multiple ROIs on HDR port only.
    # cam.readout_port = 'Dynamic Range'
    # cam.speed = 0
    # cam.gain = 1

    cam.metadata_enabled = True

    max_roi_count = cam.get_param(const.PARAM_ROI_COUNT, const.ATTR_MAX)
    cam.set_roi(0, 0, 250, 250)
    if max_roi_count > 1:
        cam.set_roi(300, 100, 300, 500)
        cam.set_roi(650, 300, 300, 200)
    else:
        print(f'Camera only supports {max_roi_count} ROI(s) for current port/speed/gain')

    cam.start_seq(exp_time=20, num_frames=NUM_FRAMES)

    cnt = 0
    while cnt < NUM_FRAMES:
        frame, fps, frame_count = cam.poll_frame()
        cnt += 1

        num_rois = len(frame['pixel_data']) if len(cam.rois) > 1 else 1
        for roi_index in range(num_rois):
            pixel_data = frame['pixel_data'][roi_index] if len(cam.rois) > 1 \
                else frame['pixel_data']
            low = np.amin(pixel_data)
            high = np.amax(pixel_data)
            avg = np.average(pixel_data)
            s1 = frame['meta_data']['roi_headers'][roi_index]['roi']['s1']
            s2 = frame['meta_data']['roi_headers'][roi_index]['roi']['s2']
            p1 = frame['meta_data']['roi_headers'][roi_index]['roi']['p1']
            p2 = frame['meta_data']['roi_headers'][roi_index]['roi']['p2']
            print(f'ROI: {roi_index} ({s1:3}, {p1:3}) ({s2:3}, {p2:3})'
                  f'\tMin: {low}\tMax: {high}\tAverage: {avg:.0f}'
                  f'\tFrames: {frame_count}\tFrame Rate: {fps:.1f}')

            alpha = 2.0  # Contrast control (1.0-3.0)
            beta = 10  # Brightness control (0-100)
            img = cv2.convertScaleAbs(pixel_data, alpha=alpha, beta=beta)

            window_name = f'Region: {roi_index}'
            cv2.imshow(window_name, img)
            cv2.moveWindow(window_name, s1, p1)

        cv2.waitKey(0)

    cam.finish()

    cam.close()
    pvc.uninit_pvcam()

    print(f'\nTotal frames: {cnt}')


if __name__ == "__main__":
    main()
