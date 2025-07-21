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
        self.dim = (100, 100)
        self.pixel_data = None

    def run(self):
        self.cam.open()
        self.append_output(f'Camera Opened: {self.cam.name}'
                           f'\tSensor Size: {self.cam.sensor_size}')

        width = 400
        height = int(self.cam.sensor_size[1] * width / self.cam.sensor_size[0])
        self.dim = (width, height)

        cnt = 0

        self.cam.start_live(exp_time=20)

        while cnt < NUM_FRAMES:
            frame, fps, frame_count = self.cam.poll_frame()
            cnt += 1

            self.pixel_data = cv2.resize(frame['pixel_data'], self.dim,
                                         interpolation=cv2.INTER_AREA)
            # Cannot init windows from non-GUI threads
            # cv2.imshow(self.cam.name, self.pixel_data)
            # cv2.waitKey(10)

            self.append_output(f'Camera: {self.cam.name}\tFrame Rate: {fps:5.1f}'
                               f'  Frames: {frame_count:3}  Returned Count: {cnt:3}')

        self.cam.finish()
        self.cam.close()

        self.pixel_data = None
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
                if thread.pixel_data is not None:
                    cv2.imshow(thread.cam.name, thread.pixel_data)
                    cv2.resizeWindow(thread.cam.name, thread.dim[0], thread.dim[1])
                    cv2.waitKey(10)

    pvc.uninit_pvcam()

    if len(camera_names) > 0:
        print('All cameras complete')
        print('To exit, close any image window, or press any key while an image window focused...')
        close = False
        while not close:
            for camera_name in camera_names:
                if cv2.getWindowProperty(camera_name, cv2.WND_PROP_VISIBLE) < 1:
                    close = True
                    break
            if not close:
                key = cv2.waitKey(100)
                close = key != -1
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
