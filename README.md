# PyVCAM Wrapper

PyVCAM Wrapper is a Python3.X wrapper for the PVCAM SDK.

## Getting Started
Follow the instructions below to get PyVCAM up and running on your machine for
development and testing.

### Prerequisites
* An understanding of PVCAM is very helpful for understanding PyVCAM.
* A C/C++ compiler is needed to build native source code.
  For Windows, the compilers from Visual Studio 2019 and 2022 were used for testing.
* The newest version of Python 3 which can be downloaded
  [here](https://www.python.org/downloads/).
* The latest PVCAM and PVCAM SDK which can be downloaded
  [here](https://www.teledynevisionsolutions.com/products/pvcam-sdk-amp-driver/).
* PyVCAM was developed and tested using Microsoft Windows 10/64-bit and Windows 11.
  Linux is also supported, but testing has been minimal.

### Installation
When you are ready to install the wrapper use your command prompt to navigate into the directory
that contains `pyproject.toml` file and run ```python -m pip install .``` (don't forget the dot
at the end) 

### How to use the wrapper

#### Create Camera Example
This will create a camera object using the first camera that is found that can then be used
to interact with the camera.
```
from pyvcam import pvc 
from pyvcam.camera import Camera   

pvc.init_pvcam()                   # Initialize PVCAM 
cam = next(Camera.detect_camera()) # Use generator to find first camera
cam.open()                         # Open the camera
```

#### Single Image Example
This captures a single image with a 20 ms exposure time and prints the values of the first 5 pixels.
```
# A camera object named cam has already been created
frame = cam.get_frame(exp_time=20)
print("First five pixels of frame: {}, {}, {}, {}, {}".format(*frame[:5]))
```

#### Changing Settings Example
This is an example of how to change some of the settings on the cameras.
```
cam.clear_mode = "Never"
cam.exp_mode = "Ext Trig Trig First"
```
The same setting can be done without the strings via predefined constants that are generated
from C header `pvcam.h`:
```
from pyvcam import constants as const

cam.clear_mode = const.CLEAR_NEVER
cam.exp_mode = const.EXT_TRIG_TRIG_FIRST
```

#### Changing Speed Table Settings Example
When changing speed table, set new value to all three properties in this exact order:
`readout_port` &#8594; `speed` &#8594; `gain`, otherwise some legacy cameras could
end up in invalid state.
```
cam.readout_port = 0
cam.speed = 0
cam.gain = 1
```

More information on how to use this wrapper and how it works can be found in
[docs/PyVCAM Wrapper.md](https://github.com/Photometrics/PyVCAM/blob/master/docs/PyVCAM%20Wrapper.md).
