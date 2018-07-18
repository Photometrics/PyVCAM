# PyVCAM Wrapper

PyVCAM Wrapper is a Python3.X wrapper for the PVCAM SDK

## Getting Started
Follow the bellow instructions to get PyVCAM up and running on your machine for development and testing


### Prerequisites
* Note that a good understanding of PVCAM is very helpful for understandingPyVCAM.
* The wrapper needs to be compiled so it will be necessary to have a C/C++ compiler to install this application
* You will need to have the newest version of Python 3 installed on your machine which can be downloaded [here](https://www.python.org/downloads/)
* You will also need to have the PVCam SDK installed this can be downloaded [here](https://www.photometrics.com/support/software/#software).


### Installing
When you are ready to install the wrapper use your command prompt to navigate into the directory that contains 
setup.py and run ```python setup.py install``` 


### How to use the wrapper
#### Create Camera Example
This will create a camera object using the first camera that is found that can then be used to interact with the camera
```
from pyvcam import pvc 
from pyvcam.camera import Camera   

pvc.init_pvcam()                   # Initialize PVCAM 
cam = next(Camera.detect_camera()) # Use generator to find first camera. 
cam.open()                         # Open the camera.
```

#### Single Image Example
This captures a single image with a 20 ms exposure time and prints the values of the first 5 pixels
```
# A camera object named cam has already been created
frame = cam.get_frame(exp_time=20)
print("First five pixels of frame: {}, {}, {}, {}, {}".format(*frame[:5]))
```

#### Changing Settings Example
This is an example of how to change some of the settings on the cameras
```
# A camera object named camera has already been created
camera.clear_mode = "Never"
camera.exp_mode = "Ext Trig Trig First"
camera.readout_port = 0
camera.speed_table_index = 0
camera.gain = 1
```

More information on how to use this wrapper and how it works can be found [here](https://github.com/Photometrics/PyVCAM/blob/master/Documents/PyVCAM%20Wrapper.md)
