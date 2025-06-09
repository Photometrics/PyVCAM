"""change_settings_test.py

Note: Written for Prime 2020; some settings may not be valid for other cameras!
Change settings in the `camera_settings.py` file.
"""
# pylint: disable=c-extension-no-member

from camera_settings import apply_settings

from pyvcam import pvc
from pyvcam.camera import Camera


def print_settings(camera):
    print(f"  Clear mode: {camera.clear_mode} '{camera.clear_modes[camera.clear_mode]}'")
    print(f"  Exposure mode: {camera.exp_mode} '{camera.exp_modes[camera.exp_mode]}'")
    print(f"  Readout port: {camera.readout_port} '{camera.readout_ports[camera.readout_port]}'")
    print(f"  Speed: {camera.speed_table_index} '{camera.speed_name}'")
    print(f"  Gain: {camera.gain} '{camera.gain_name}'")


def main():
    # Initialize PVCAM
    pvc.init_pvcam()

    # Find the first available camera.
    camera = next(Camera.detect_camera())
    camera.open()

    # Show camera name and speed table.
    print(camera)

    print("\nBefore changing settings:")
    print_settings(camera)

    # Change the camera settings from the separate file.
    apply_settings(camera)

    print("\nAfter changing settings:")
    print_settings(camera)

    # Cleanup before exit
    camera.close()
    pvc.uninit_pvcam()


if __name__ == "__main__":
    main()
