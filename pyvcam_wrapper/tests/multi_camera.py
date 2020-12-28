import cv2
import threading

from pyvcam import pvc
from pyvcam.camera import Camera

NUM_FRAMES = 100

class TriggerThreadRun (threading.Thread):
    def __init__(self, cam):
        threading.Thread.__init__(self)
        self.cam = cam
        self.output = ''

    def run(self):
        cnt = 0

        self.cam.open()
        self.appendOutput('Camera Opened. Name: ' + self.cam.name + ' Sensor Size: ' + str(self.cam.sensor_size))

        width = 400
        height = int(self.cam.sensor_size[1] * width / self.cam.sensor_size[0])
        dim = (width, height)

        self.cam.start_live(exp_time=20)

        while cnt < NUM_FRAMES:

            frame, fps, frame_count = self.cam.poll_frame()

            pixelData = cv2.resize(frame['pixel_data'], dim, interpolation=cv2.INTER_AREA)
            cv2.imshow(self.cam.name, pixelData)
            cv2.waitKey(10)

            if self.output != '':
                self.output += '\n'

            self.appendOutput('CamName:{}\tFrame Rate: {:.1f}\tFrame Count: {:.0f}\tReturned Count: {:.0f}'.format(self.cam.name, fps, frame_count, cnt))
            cnt += 1

        self.cam.finish()
        self.cam.close()

        self.appendOutput('Camera Closed. Name: ' + self.cam.name)

    def appendOutput(self, outputLine):
        if self.output != '':
            self.output += '\n'

        self.output += outputLine

    def getOutput(self):
        ret = self.output
        self.output = ''
        return ret

def main():
    pvc.init_pvcam()

    camera_names = Camera.get_available_camera_names()
    print('Available cameras: ' + str(camera_names))

    threadList = []
    for cameraName in camera_names:
        cam = Camera.select_camera(cameraName)
        thread = TriggerThreadRun(cam)
        thread.start()
        threadList.append(thread)

    checkComplete = False
    while not checkComplete:
        checkComplete = True
        for thread in threadList:
            checkComplete &= not thread.is_alive()

            threadOutput = thread.getOutput()
            if threadOutput != '':
                print(threadOutput)

    print('All cameras complete')
    pvc.uninit_pvcam()

if __name__ == "__main__":
    main()
