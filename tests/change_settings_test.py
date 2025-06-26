"""change_settings_test.py

Note: Written for Prime 2020; some settings may not be valid for other cameras!
Change settings in the `camera_settings.py` file.
"""

from camera_settings import apply_settings

from pyvcam import pvc
from pyvcam.camera import Camera


def print_settings(cam):
    print(f"  Clear mode: {cam.clear_mode} '{cam.clear_modes[cam.clear_mode]}'")
    print(f"  Exposure mode: {cam.exp_mode} '{cam.exp_modes[cam.exp_mode]}'")
    print(f"  Readout port: {cam.readout_port} '{cam.readout_ports[cam.readout_port]}'")
    print(f"  Speed: {cam.speed} '{cam.speed_name}'")
    print(f"  Gain: {cam.gain} '{cam.gain_name}'")


def main():
    # Initialize PVCAM
    pvc.init_pvcam()

    # Find the first available camera.
    cam = next(Camera.detect_camera())
    cam.open()

    # Show camera name and speed table.
    print(cam)

    print("\nBefore changing settings:")
    print_settings(cam)

    # Change the camera settings from the separate file.
    apply_settings(cam)

    print("\nAfter changing settings:")
    print_settings(cam)

    # Cleanup before exit
    cam.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
