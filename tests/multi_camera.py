import threading

import cv2

from pyvcam import pvc
from pyvcam.camera import Camera

NUM_FRAMES = 100


class TriggerThreadRun(threading.Thread):
    def __init__(self, cam):
        threading.Thread.__init__(self)
        self.cam = cam
        self.output = ''

    def run(self):
        self.cam.open()
        self.append_output(f'Camera Opened: {self.cam.name}'
                           f'\tSensor Size: {self.cam.sensor_size}')

        width = 400
        height = int(self.cam.sensor_size[1] * width / self.cam.sensor_size[0])
        dim = (width, height)

        cnt = 0

        self.cam.start_live(exp_time=20)

        while cnt < NUM_FRAMES:
            frame, fps, frame_count = self.cam.poll_frame()
            cnt += 1

            pixel_data = cv2.resize(frame['pixel_data'], dim,
                                    interpolation=cv2.INTER_AREA)
            cv2.imshow(self.cam.name, pixel_data)
            cv2.waitKey(10)

            self.append_output(f'Camera: {self.cam.name}\tFrame Rate: {fps:5.1f}'
                               f'  Frame Count: {frame_count:3}  Returned Count: {cnt:3}')

        self.cam.finish()
        self.cam.close()

        self.append_output(f'Camera Closed: {self.cam.name}')

    def append_output(self, output_line):
        if self.output != '':
            self.output += '\n'
        self.output += output_line

    def get_output(self):
        ret = self.output
        self.output = ''
        return ret


def main():
    pvc.init_pvcam()

    camera_names = Camera.get_available_camera_names()
    print(f'Available cameras: {camera_names}')

    thread_list = []
    for camera_name in camera_names:
        cam = Camera.select_camera(camera_name)
        thread = TriggerThreadRun(cam)
        thread.start()
        thread_list.append(thread)

    check_complete = False
    while not check_complete:
        check_complete = True
        for thread in thread_list:
            check_complete &= not thread.is_alive()

            thread_output = thread.get_output()
            if thread_output != '':
                print(thread_output)

    print('All cameras complete')
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
