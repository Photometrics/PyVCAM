"""camera_settings.py

Note: Currently not used. Proof of concept to show how to change camera settings
in one file and have them applied to a camera from a different script.
"""

from pyvcam import constants as const


def apply_settings(camera):
    """Changes the settings of a camera."""
    camera.clear_mode = const.CLEAR_NEVER
    camera.exp_mode = "Internal Trigger"
    camera.readout_port = 0
    camera.speed_table_index = 0
    camera.gain = 1
