"""camera_settings.py

Note: Currently not used. Proof of concept to show how to change camera settings
in one file and have them applied to a camera from a different script.
"""

def apply_settings(camera):
    """Changes the settings of a camera."""
    camera.clear_mode = 0
    camera.exp_mode = "Internal Trigger"
    camera.readout_port = 0
    camera.speed_table_index = 0
    camera.gain = 1
