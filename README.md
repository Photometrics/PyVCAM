# PyVCAM Wrapper

PyVCAM Wrapper is a Python3.X wrapper for the PVCAM SDK.

## Getting Started
Follow the instructions below to get PyVCAM up and running on your machine for development and testing.


### Prerequisites
* An understanding of PVCAM is very helpful for understanding PyVCAM.
* A C/C++ compiler is needed to build native source code. For Windows, MSVC 1928 was used for testing.
* The newest version of Python 3 which can be downloaded [here](https://www.python.org/downloads/).
* The latest PVCAM and PVCAM SDK which can be downloaded [here](https://www.photometrics.com/support/software/#software).
* PyVCAM was developed and tested using Microsoft Windows 10/64-bit. The build package also supports Linux, but testing has been minimal.

<<<<<<< HEAD

### Installing
When you are ready to install the wrapper use your command prompt to navigate into the directory that contains 
setup.py and run ```python setup.py install``` 


### How to use the wrapper
#### Create Camera Example
This will create a camera object using the first camera that is found that can then be used to interact with the camera.
```
from pyvcam import pvc 
from pyvcam.camera import Camera   

pvc.init_pvcam()                   # Initialize PVCAM 
cam = next(Camera.detect_camera()) # Use generator to find first camera. 
cam.open()                         # Open the camera.
```

=======
## ARTIQ Integration with [Network Device Support Packages (NDSP)](https://m-labs.hk/artiq/manual/developing_a_ndsp.html)
### Initial Setup
1. Clone this repository into your Projects folder with the command `git clone git@github.com:quantumion/PyVCAM.git`.
* **Note:** The following steps (2-3) may be skipped if installation is to be done in the default/user specified environment - virtual environment usage is not a requirement.
2. Navigate into your PyVCAM folder and create a virtual environment with `virtualenv venv`. 
3. Activate your virtual environment with `source venv/bin/activate`.
4. Install PyVCAM by running `pip install .`.

### Controller Activation
* Run `python -m pyvcam`.
* Add flags to specify parameters. Add `-h` to see a list of flags.
* To specify a port, run with `python -m pyvcam -p <port number>`. The default port is `3249`.
    * **NOTE:** You need to modify your `device_db.py` file if you have changed the port number.
    
### ARTIQ Experiment Setup
* In your `device_db.py` file, you will need to specify a remote port that your environment will connect to, as specified in the [NDSP](https://m-labs.hk/artiq/manual/developing_a_ndsp.html) guide.
```
device_db = {
    "pyvcam": {
        "type": "controller",
        "host": "::1", # Change this for a different host
        "port": 3249, # Change this for a different port number
        "command": "python -m pyvcam -p {port}"
    }
    ... # The rest of your device_db
}
```
* In every ARTIQ experiment, you will need to initialize a pyvcam device within the `build` function.
```
def build(self):
    self.setattr_device("pyvcam")
```

### Usage
* Camera functions run like a normal function with `self.pyvcam.<function>()`.
* Functions with argument parameters may be input into parenthesis.
* **Due to the implementation of the driver class, functions must also be modified from their default style. Follow the style in the "Driver Wrapper" columns as shown below.**
* Example usage given that a camera object cam/self.pyvcam has already been created:

| Bare PyVCAM Class | Driver Wrapper                  | Result                           |
|-------------------|---------------------------------|----------------------------------|
| `print(cam.gain)` | `print(self.pyvcam.get_gain())` | Prints current gain value as int |
| `cam.gain = 1`    | `self.pyvcam.set_gain(1)`       | Sets gain value to 1             |


## How to use the wrapper
>>>>>>> f078cf0b6f249618d314a15bb498640c467f508b
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
# A camera object named cam has already been created
cam.clear_mode = "Never"
cam.exp_mode = "Ext Trig Trig First"
cam.readout_port = 0
cam.speed_table_index = 0
cam.gain = 1
```

<<<<<<< HEAD
More information on how to use this wrapper and how it works can be found [here](https://github.com/Photometrics/PyVCAM/blob/master/docs/PyVCAM%20Wrapper.md).
=======
| Bare PyVCAM Class                   | Driver Wrapper                                 |
|-------------------------------------|------------------------------------------------|
| `cam.exp_mode = "Internal Trigger"` | `self.pyvcam.set_exp_mode("Internal Trigger")` |
| `cam.readout_port = 0`              | `self.pyvcam.set_readout_port(0)`              |
| `cam.speed_table_index = 0`         | `self.pyvcam.set_speed_table_index(0)`         |
| `cam.gain = 1`                      | `self.pyvcam.set_gain(1)`                      |

More information on how to use this wrapper and how it works can be found [here](https://github.com/Photometrics/PyVCAM/blob/master/docs/PyVCAM%20Wrapper.md).

## Generating Documentation Using [Sphinx](https://www.sphinx-doc.org/en/master/index.html)
### [Installing Sphinx from PyPI](https://www.sphinx-doc.org/en/master/usage/installation.html#installation-from-pypi)
1. Install Sphinx by entering `pip install -U sphinx` in your terminal.
2. After installation, type `sphinx-build --version` in your terminal. A successful installation will return a version number.

### Generating Documentation
1. Ensure the PyVCAM Package is already installed in your environment as shown in the [Initial Setup](#initial-setup). This is important as the Sphinx configuration will read directly from the installation.
2. Navigate to the `docs/` folder.
3. Run the command `make html`. This reads the `.rst` files in `docs/source/` and automatically generates documentation in `html` format in `docs/build/`.
>>>>>>> f078cf0b6f249618d314a15bb498640c467f508b
