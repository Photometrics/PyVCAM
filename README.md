# PyVCAM

[![GitHub release](https://img.shields.io/github/v/release/Photometrics/PyVCAM?label=GitHub%20stable&color=green)](https://github.com/Photometrics/PyVCAM/releases "GitHub stable release")
[![GitHub release](https://img.shields.io/github/v/release/Photometrics/PyVCAM?include_prereleases&label=GitHub%20latest)](https://github.com/Photometrics/PyVCAM/releases "GitHub latest release")
![GitHub License](https://img.shields.io/github/license/Photometrics/PyVCAM)  
[![PyPI release](https://img.shields.io/pypi/v/PyVCAM?label=PyPI&&color=green)](https://pypi.org/project/PyVCAM/ "PyPI release")
[![TestPyPI release](https://img.shields.io/pypi/v/PyVCAM?pypiBaseUrl=https%3A%2F%2Ftest.pypi.org&label=TestPyPI)](https://test.pypi.org/project/PyVCAM/ "TestPyPI release")
[![Python version from PEP 621 TOML](https://img.shields.io/python/required-version-toml?tomlFilePath=https%3A%2F%2Fraw.githubusercontent.com%2FPhotometrics%2FPyVCAM%2Fmaster%2Fpyproject.toml)](https://python.org "Python versions")  
[![Tests status](https://github.com/Photometrics/PyVCAM/actions/workflows/tests.yml/badge.svg)](https://github.com/Photometrics/PyVCAM/actions/workflows/tests.yml "Tests status")
[![Average time to resolve an issue](http://isitmaintained.com/badge/resolution/Photometrics/PyVCAM.svg)](http://isitmaintained.com/project/Photometrics/PyVCAM "Average time to resolve an issue")
[![Percentage of issues still open](http://isitmaintained.com/badge/open/Photometrics/PyVCAM.svg)](http://isitmaintained.com/project/Photometrics/PyVCAM "Percentage of issues still open")
---

PyVCAM is a Python 3.x wrapper for the PVCAM library.

## Installation
The wrapper can be installed using the following command in an environment with
`python` and `pip`:
```
pip install PyVCAM
```
It requires the latest PVCAM to be installed - can be downloaded
[here](https://www.teledynevisionsolutions.com/products/pvcam-sdk-amp-driver/).

The binary packages are available for 64-bit Windows and Linux on Intel platform only.
For 32-bit or Arm Linux platform it must be complied from the source code.
Please follow the instructions for development below.

## Examples
The installed package contains just the wrapper. If you want to get an image from
the camera right away before writing your own application, PyVCAM repository contains
multiple examples, like `seq_mode.py` or `live_mode.py`.

Get a local copy of the GitHub repository by downloading the zip archive or cloning it
e.g. from git command prompt:
```
git clone https://github.com/Photometrics/PyVCAM.git PyVCAM
```
Then switch to the folder with examples and run one:
```
cd PyVCAM/examples
python seq_mode.py
```
Some examples show the image from camera in a UI window and require `opencv-python`
or `matplotlib` packages to be installed.

## Development
For troubleshooting or active contribution you may want to install PyVCAM from GitHub.

### Prerequisites
* Cloned repository from GitHub as described in Examples chapter above.
* A C/C++ compiler is needed to build native source code.
  On Windows, Visual Studio 2022 was used for testing.
* The latest PVCAM and PVCAM SDK - both can be downloaded
  [here](https://www.teledynevisionsolutions.com/products/pvcam-sdk-amp-driver/).

### Installation
Use your command prompt to navigate into the directory that contains `pyproject.toml`
file and run:
```
pip install .
``` 

## How to use the wrapper
An understanding of PVCAM API is very helpful for understanding PyVCAM.

### Create Camera Example
This will create a camera object using the first camera that is found that can then be used
to interact with the camera.
```
from pyvcam import pvc 
from pyvcam.camera import Camera   

pvc.init_pvcam()                   # Initialize PVCAM 
cam = next(Camera.detect_camera()) # Use generator to find first camera
cam.open()                         # Open the camera
```

### Single Image Example
This captures a single image with a 20 ms exposure time and prints the values of the first 5 pixels.
```
# A camera object named cam has already been created
frame = cam.get_frame(exp_time=20)
print("First five pixels of frame: {}, {}, {}, {}, {}".format(*frame[:5]))
```

### Changing Settings Example
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

### Changing Speed Table Settings Example
When changing speed table, set new value to all three properties in this exact order:
`readout_port` &#8594; `speed` &#8594; `gain`, otherwise some legacy cameras could
end up in invalid state.
```
cam.readout_port = 0
cam.speed = 0
cam.gain = 1
```

More information on how to use this wrapper and how it works can be found in
[docs/PyVCAM.md](https://github.com/Photometrics/PyVCAM/blob/master/docs/PyVCAM.md).
