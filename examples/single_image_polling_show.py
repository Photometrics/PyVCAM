from matplotlib import pyplot as plt

from pyvcam import pvc
from pyvcam.camera import Camera


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    frame = cam.get_frame(exp_time=20)

    plt.imshow(frame.reshape(cam.sensor_size[::-1]), cmap='gray')
    plt.show()

    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
