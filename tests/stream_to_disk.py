import numpy as np
import time

from pyvcam import pvc
from pyvcam import constants as const
from pyvcam.camera import Camera

NUM_FRAMES = 200
FRAME_DATA_PATH = r'data.bin'
BUFFER_FRAME_COUNT = 16
WIDTH = 1000
HEIGHT = 1000

# Warning. This test can only succeed if the camera supports meta_data since that is needed to confirm frames were not dropped

def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    cam.meta_data_enabled = True
    cam.set_roi(0, 0, WIDTH, HEIGHT)
    cam.start_live(exp_time=100, buffer_frame_count=BUFFER_FRAME_COUNT, stream_to_disk_path=FRAME_DATA_PATH)

    # Data is streamed to disk in a C++ call-back function invoked directly by PVCAM. To not overburden the system,
    # only poll for frames in python at a slow rate, then exit when the frame count indicates all frames have been
    # written to disk
    while True:
        frame, fps, frame_count = cam.poll_frame()

        if frame_count >= NUM_FRAMES:
            low = np.amin(frame['pixel_data'])
            high = np.amax(frame['pixel_data'])
            average = np.average(frame['pixel_data'])
            print('Min:{}\tMax:{}\tAverage:{:.0f}\tFrame Count:{:.0f} Frame Rate: {:.1f}'.format(low, high, average, frame_count, fps))
            break

        time.sleep(1)

    cam.finish()

    imageFormat = cam.get_param(const.PARAM_IMAGE_FORMAT)
    if imageFormat == const.PL_IMAGE_FORMAT_MONO8:
        BYTES_PER_PIXEL = 1
    else:
        BYTES_PER_PIXEL = 2

    cam.close()
    pvc.uninit_pvcam()

    # Read out meta data from stored frames
    FRAME_ALIGNMENT = 0                   # Typically 0. 32 for Kinetix PCIe
    FRAME_BUFFER_ALIGNMENT = 4096
    META_DATA_FRAME_HEADER_SIZE = 48
    META_DATA_ROI_SIZE = 32
    META_DATA_SIZE = META_DATA_FRAME_HEADER_SIZE + META_DATA_ROI_SIZE
    pixelsPerFrame = WIDTH * HEIGHT
    bytesPerFrameUnpadded = pixelsPerFrame * BYTES_PER_PIXEL + META_DATA_SIZE
    framePad = 0 if FRAME_ALIGNMENT == 0 else FRAME_ALIGNMENT - (bytesPerFrameUnpadded % FRAME_ALIGNMENT)
    bytesPerFrame = bytesPerFrameUnpadded + framePad
    frameBufferAlignmentPad = FRAME_BUFFER_ALIGNMENT - ((BUFFER_FRAME_COUNT * bytesPerFrame) % FRAME_BUFFER_ALIGNMENT)

    with open(FRAME_DATA_PATH, "rb") as f:

        badMetaDataCount = 0
        for frame_index in range(NUM_FRAMES):
            frame_number = frame_index + 1

            # Read frame number from meta data header
            # Every time the circular buffer wraps around, bytes are padded into the file. This is a result of needing to
            # write data in chunks that are multiples of the alignment boundary.
            frameBufferPad = int(frame_index / BUFFER_FRAME_COUNT) * frameBufferAlignmentPad

            FRAME_NUMBER_OFFSET = 5
            filePos = frame_index * bytesPerFrame + FRAME_NUMBER_OFFSET + frameBufferPad
            f.seek(filePos, 0)
            frameNumberBytes = f.read(4)

            frame_number_meta_data = int.from_bytes(frameNumberBytes, 'little')

            if frame_number != frame_number_meta_data:
                badMetaDataCount += 1
                print('Expected: ' + str(frame_number) + ' Observed: ' + str(frame_number_meta_data))

    print('Verified ' + str(NUM_FRAMES) + ' frames:')
    print('  Meta data errors: ' + str(badMetaDataCount))

    if badMetaDataCount > 0:
        print('\nMeta data error troubleshooting:')
        print('  1. Verify FRAME_ALIGNMENT for camera and interface')
        print('  2. Increase exposure time')

if __name__ == "__main__":
    main()
