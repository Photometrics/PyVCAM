from pyvcam import pvc
from pyvcam.camera import Camera


def main():
    # Initialize PVCAM and find the first available camera.
    pvc.init_pvcam()
    cam = [cam for cam in Camera.detect_camera()][0]
    cam.open()
    cam.speed_table_index = 0
    for i in range(5):
        frame = cam.get_frame(exp_time=20)
        print("First five pixels of frame: {}, {}, {}, {}, {}".format(*frame[:5]))
    cam.close()
    pvc.uninit_pvcam()

if __name__ == "__main__":
    main()
