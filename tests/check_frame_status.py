import time
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()

    frame_status = 'READOUT_FAILED'

    # Check status from sequence collect
    cam.start_seq(exp_time=250, num_frames=2)

    acq_in_progress = True
    while acq_in_progress:
        frame_status = cam.check_frame_status()
        print(f'Sequence frame status: {frame_status}')

        if frame_status in ('READOUT_NOT_ACTIVE', 'FRAME_AVAILABLE',
                            'READOUT_COMPLETE', 'READOUT_FAILED'):
            acq_in_progress = False

        if acq_in_progress:
            time.sleep(0.05)

    if frame_status != 'READOUT_FAILED':
        frame, fps, frame_count = cam.poll_frame()

        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        avg = np.average(frame['pixel_data'])
        print(f'Min: {low}\tMax: {high}\tAverage: {avg:.0f}\tFrames: {frame_count}'
              f'\tFrame Rate: {fps:.1f}')

    cam.finish()

    frame_status = cam.check_frame_status()
    print(f'Sequence frame status after acquisition: {frame_status}\n')

    # Check status from live collect. Status will only report FRAME_AVAILABLE
    # between acquisitions, so an indeterminate number of frames are needed
    # before we catch the FRAME_AVAILABLE status
    cam.start_live(exp_time=10)

    acq_in_progress = True
    while acq_in_progress:
        frame_status = cam.check_frame_status()
        print('Live frame status: ' + frame_status)

        if frame_status in ('READOUT_NOT_ACTIVE', 'FRAME_AVAILABLE', 'READOUT_FAILED'):
            acq_in_progress = False

        if acq_in_progress:
            time.sleep(0.05)

    if frame_status != 'READOUT_FAILED':
        frame, fps, frame_count = cam.poll_frame()

        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        avg = np.average(frame['pixel_data'])
        print(f'Min: {low}\tMax: {high}\tAverage: {avg:.0f}\tFrames: {frame_count}'
              f'\tFrame Rate: {fps:.1f}')

    cam.finish()

    frame_status = cam.check_frame_status()
    print(f'Live frame status after acquisition: {frame_status}')

    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
