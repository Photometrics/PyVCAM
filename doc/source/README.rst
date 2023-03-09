README
============

`ARTIQ`_ `Network Device Support Package (NDSP)`_ Integration for the `PyVCAM Wrapper`_

.. _ARTIQ: http://m-labs.hk/experiment-control/artiq/
.. _Network Device Support Package (NDSP): https://m-labs.hk/artiq/manual/developing_a_ndsp.html
.. _PyVCAM Wrapper: https://github.com/Photometrics/PyVCAM

PyVCAM Wrapper is a Python3.X wrapper for the PVCAM SDK.

Getting Started
---------------
Follow the instructions below to get PyVCAM up and running on your machine for development and testing.


Prerequisites
^^^^^^^^^^^^^
* An understanding of PVCAM is very helpful for understanding PyVCAM.
* A C/C++ compiler is needed to build native source code. For Windows, MSVC 1928 was used for testing.
* The newest version of Python 3 which can be downloaded `here <https://www.python.org/downloads/>`__.
* The latest PVCAM and PVCAM SDK which can be downloaded `here <https://www.photometrics.com/support/software/#software>`__.
* PyVCAM was developed and tested using Microsoft Windows 10/64-bit. The build package also supports Linux, but testing has been minimal.

ARTIQ Integration with NDSPs
------------------------------------------------------------------------------------------------------------------------

Initial Setup
^^^^^^^^^^^^^

1. Clone the `sipyco <https://github.com/m-labs/sipyco>`_ repository into your Projects folder with the command ``git clone git@github.com:m-labs/sipyco.git``.
2. Clone this repository into your Projects folder with the command ``git clone git@github.com:quantumion/PyVCAM.git``.
3. Navigate into your PyVCAM folder and create a virtual environment with ``virtualenv venv``. 
4. Activate your virtual environment with ``source venv/bin/activate``. Install numpy by running ``pip install numpy``.
    * **Note:** Steps 3-4 may be skipped if installation is to be done on disk.
5. Navigate into your sipyco directory, and run ``pip install .``.
    * This installs the sipyco package into your PyVCAM environment.
6. Activate the PyVCAM setup process by running ``python setup.py install``.

Controller Activation
---------------------

* Navigate to ``/src/`` folder and run ``python -m pyvcam``.
* Add flags to specify parameters. Add ``-h`` to see a list of flags.
* To specify a port, run with ``python -m pyvcam -p <port number>``. The default port is ``3249``.
    * **NOTE:** You need to modify your ``device_db.py`` file if you have changed the port number.
    
.. _artiq-setup:

ARTIQ Experiment Setup
^^^^^^^^^^^^^^^^^^^^^^

* In your ``device_db.py`` file, you will need to specify a remote port that your environment will connect to, as specified in the `NDSP <https://m-labs.hk/artiq/manual/developing_a_ndsp.html>`_ guide.

.. code-block.:: python
    
    device_db = {
        "pyvcam": {
            "type": "controller",
            "host": "::1",
            "port": 3249, # Change this for a different port number
            "command": "python ~/Projects/QuantumIon-ARTIQ-System/pyvcam/aqctl_pyvcam.py -p {port}"
        }
        ... # The rest of your device_db
    }

* In every ARTIQ experiment, you will need to initialize a pyvcam device within the ``build`` function.

.. code-block:: python

    def build(self):
        self.setattr_device("pyvcam")

* Run a Nix environment with ``nix develop``.
* You can now run files with the ``artiq_run`` command.

Usage
^^^^^

* Camera functions run like a normal function with ``self.pyvcam.<function>()``.
* Functions with argument parameters may be input into parenthesis.
* Example usage:

.. code-block:: python

    print(self.pyvcam.gain())           # prints current gain value as int
    self.pyvcam.set_gain(1)             # sets gain value to 1

How to use the wrapper
^^^^^^^^^^^^^^^^^^^^^^
**Single Image Example**

This captures a single image with a 20 ms exposure time and prints the values of the first 5 pixels.

.. code-block::

    # A camera object self.pyvcam has already been created
    frame = self.pyvcam.get_frame(exp_time=20)
    print("First five pixels of frame: {}, {}, {}, {}, {}".format(*frame[:5]))

**Changing Settings Example**

This is an example of how to change some of the settings on the cameras.

.. code-block::

    # A camera object self.pyvcam has already been created
    self.pyvcam.set_exp_mode(1792)  # sets the exposure mode to be 'Internal Trigger'
    self.pyvcam.set_readout_port(0)
    self.pyvcam.set_speed_table_index(0)
    self.pyvcam.set_gain(1)


More information on how to use this wrapper and how it works can be found `here <https://github.com/Photometrics/PyVCAM/blob/master/docs/PyVCAM%20Wrapper.md>`__.