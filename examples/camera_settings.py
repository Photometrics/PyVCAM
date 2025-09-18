"""camera_settings.py

Note: Currently not used. Proof of concept to show how to change camera settings
in one file and have them applied to a camera from a different script.
"""

from pyvcam import constants as const


def apply_settings(cam):
    """Changes the settings of a camera."""

    cam.clear_mode = const.CLEAR_NEVER
    cam.exp_mode = "Internal Trigger"

    # When changing anything in speed table it is strongly recommended to set
    # all 3 properties (readout_port, speed, gain) in predefined order.
    # After changing `readout_port` re-apply the value of `speed`.
    # After changing `speed` re-apply the `gain` value.
    cam.readout_port = 0
    cam.speed = 0
    cam.gain = 1
