import time
import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera

def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()

    cnt = 0

    # Check status from sequence collect
    cam.start_seq(exp_time=1000, num_frames=2)

    acquisitionInProgress = True
    while acquisitionInProgress:

        frameStatus = cam.check_frame_status()
        print('Seq frame status: ' + frameStatus)

        if frameStatus == 'READOUT_NOT_ACTIVE' or frameStatus == 'FRAME_AVAILABLE' or frameStatus == 'READOUT_COMPLETE' or frameStatus == 'READOUT_FAILED':
            acquisitionInProgress = False

        if acquisitionInProgress:
            time.sleep(0.05)

    if frameStatus != 'READOUT_FAILED':
        frame, fps, frame_count = cam.poll_frame()

        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        average = np.average(frame['pixel_data'])

        print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Rate: {:.1f}\tFrame Count: {:.0f}'.format(low, high, average, fps, frame_count))

    cam.finish()

    frameStatus = cam.check_frame_status()
    print('Seq post-acquisition frame status: ' + frameStatus + '\n')

    # Check status from live collect. Status will only report FRAME_AVAILABLE between acquisitions, so an indeterminate number of frames are needed
    # before we catch the FRAME_AVAILABLE status
    cam.start_live(exp_time=10)

    acquisitionInProgress = True
    while acquisitionInProgress:

        frameStatus = cam.check_frame_status()
        print('Live frame status: ' + frameStatus)

        if frameStatus == 'READOUT_NOT_ACTIVE' or frameStatus == 'FRAME_AVAILABLE' or frameStatus == 'READOUT_FAILED':
            acquisitionInProgress = False

        if acquisitionInProgress:
            time.sleep(0.05)

    if frameStatus != 'READOUT_FAILED':
        frame, fps, frame_count = cam.poll_frame()

        low = np.amin(frame['pixel_data'])
        high = np.amax(frame['pixel_data'])
        average = np.average(frame['pixel_data'])

        print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Rate: {:.1f}\tFrame Count: {:.0f}'.format(low, high, average, fps, frame_count))

    cam.finish()

    frameStatus = cam.check_frame_status()
    print('Live post-acquisition frame status: ' + frameStatus)

    cam.close()

    pvc.uninit_pvcam()

    # print('Total frames: {}\n'.format(cnt))


if __name__ == "__main__":
    main()
