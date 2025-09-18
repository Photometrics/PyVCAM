import time

import numpy as np

from pyvcam import pvc
from pyvcam.camera import Camera
from pyvcam import constants as const

NUM_FRAMES = 200
FRAME_DATA_PATH = r'data.bin'
BUFFER_FRAME_COUNT = 16
WIDTH = 1000
HEIGHT = 1000

FRAME_ALIGNMENT = 0  # Typically 0, 32 for Kinetix PCIe
FRAME_BUFFER_ALIGNMENT = 4096
METADATA_FRAME_HEADER_SIZE = 48
METADATA_ROI_SIZE = 32
METADATA_SIZE = METADATA_FRAME_HEADER_SIZE + METADATA_ROI_SIZE
FRAME_NUMBER_OFFSET = 5

# Warning. This test can only succeed if the camera supports 'metadata' since
# that is needed to confirm frames were not dropped.


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    cam.metadata_enabled = True
    cam.set_roi(0, 0, WIDTH, HEIGHT)

    cam.start_live(exp_time=100, buffer_frame_count=BUFFER_FRAME_COUNT,
                   stream_to_disk_path=FRAME_DATA_PATH)

    # Data is streamed to disk in a C++ callback function invoked directly by PVCAM.
    # To not overburden the system, only poll for frames in python at a slow rate,
    # then exit when the frame count indicates all frames have been written to disk.
    while True:
        frame, fps, frame_count = cam.poll_frame()

        if frame_count >= NUM_FRAMES:
            low = np.amin(frame['pixel_data'])
            high = np.amax(frame['pixel_data'])
            avg = np.average(frame['pixel_data'])
            print(f'Min: {low}\tMax: {high}\tAverage: {avg:.0f}\tFrames: {frame_count}'
                  f'\tFrame Rate: {fps:.1f}')
            break

        time.sleep(1)

    cam.finish()

    image_format = cam.get_param(const.PARAM_IMAGE_FORMAT)
    if image_format in (const.PL_IMAGE_FORMAT_MONO8, const.PL_IMAGE_FORMAT_BAYER8):
        bytes_per_pixel = 1
    else:
        bytes_per_pixel = 2

    cam.close()
    pvc.uninit_pvcam()

    # Read out meta data from stored frames
    pixels_per_frame = WIDTH * HEIGHT
    bytes_per_frame_unpadded = pixels_per_frame * bytes_per_pixel + METADATA_SIZE
    # frame_pad must be in range from 0 to FRAME_ALIGNMENT-1
    frame_pad = 0 if FRAME_ALIGNMENT == 0 \
        else (FRAME_ALIGNMENT - (bytes_per_frame_unpadded % FRAME_ALIGNMENT)) % FRAME_ALIGNMENT
    bytes_per_frame = bytes_per_frame_unpadded + frame_pad
    frame_buffer_alignment_pad = FRAME_BUFFER_ALIGNMENT \
        - ((BUFFER_FRAME_COUNT * bytes_per_frame) % FRAME_BUFFER_ALIGNMENT)

    bad_metadata_count = 0

    with open(FRAME_DATA_PATH, "rb") as f:
        for frame_index in range(NUM_FRAMES):
            frame_number = frame_index + 1

            # Read frame number from metadata header
            # Every time the circular buffer wraps around, bytes are padded into
            # the file. This is a result of needing to write data in chunks that
            # are multiples of the alignment boundary.
            frame_buffer_pad = \
                int(frame_index / BUFFER_FRAME_COUNT) * frame_buffer_alignment_pad

            file_pos = frame_index * bytes_per_frame \
                + FRAME_NUMBER_OFFSET + frame_buffer_pad
            f.seek(file_pos, 0)
            frame_number_bytes = f.read(4)
            frame_number_metadata = int.from_bytes(frame_number_bytes, 'little')

            if frame_number != frame_number_metadata:
                bad_metadata_count += 1
                print(f'Expected: {frame_number}  Observed: {frame_number_metadata}')

    print(f'Verified {NUM_FRAMES} frames:')
    print(f'  Metadata errors: {bad_metadata_count}')

    if bad_metadata_count > 0:
        print('\nMetadata error troubleshooting:')
        print('  1. Verify FRAME_ALIGNMENT for camera and interface')
        print('  2. Increase exposure time')


if __name__ == "__main__":
    main()
