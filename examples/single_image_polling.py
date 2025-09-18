from pyvcam import pvc
from pyvcam.camera import Camera


def main():
    pvc.init_pvcam()
    cam = next(Camera.detect_camera())
    cam.open()
    print(f'Camera: {cam.name}')

    for _ in range(5):
        frame = cam.get_frame(exp_time=20)
        print(f"First five pixels: {frame[0, 0:5]}")

    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
