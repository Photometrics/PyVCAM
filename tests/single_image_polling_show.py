from pyvcam import pvc
from pyvcam.camera import Camera
from matplotlib import pyplot as plt

def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()

    frame = cam.get_frame(exp_time=20).reshape(cam.sensor_size[::-1])
    plt.imshow(frame, cmap="gray")
    plt.show()
    cam.close()
    pvc.uninit_pvcam()

if __name__=="__main__":
    main()
