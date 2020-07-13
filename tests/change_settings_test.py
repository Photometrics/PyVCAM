"""change_settings_test.py

Note: Written for Prime 2020; some settings may not be valid for other cameras!
Change settings in the `camera_settings.py` file.
"""
from pyvcam import pvc
from pyvcam.camera import Camera
import pyvcam.constants as const
from camera_settings import apply_settings

def print_settings(camera):
    print("clear mode: {}".format(const.clear_mode_name[camera.clear_mode]))
    print("exposure mode: {}".format(const.exp_mode_name[camera.exp_mode]))
    print("readout port: {}".format(camera.readout_port))
    print("speed table index: {}".format(camera.speed_table_index))
    print("gain: {}".format(camera.gain))

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
