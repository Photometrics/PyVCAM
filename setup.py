# -*- coding: utf-8 -*-
"""Build and install the pyvcam extension."""
import os
import platform
from glob import glob
from setuptools import setup, find_packages
from setuptools.extension import Extension


NAME = "pyvcam"
VERSION = "1.2.0"
DESCRIPTION = "Python wrapper for PVCAM functionality."
URL = "https://github.com/spatial-genomics/PyVCAM"
PACKAGES = find_packages()

# Check if machine OS and architecture
print("Checking machine attributes...")
is_windows = platform.system().lower().find("win") > -1
is_arch_x86_64 = platform.machine().lower().find("amd64") > -1
is_32bit = platform.machine().lower().find("x86") > -1
if not is_windows:
    print("\nERROR: This package only supports Windows machines!")
    quit()
if not (is_32bit or is_arch_x86_64):
    print(
        "\nERROR: This machine architecture is not supported, it must be "
        "amd64 or x86 32bit."
    )
    quit()

# Locate PVCAM install
try:
    pvcam_sdk_path = os.environ["PVCAM_SDK_PATH"]
except KeyError:
    print(
        "\nERROR: PVCAM_SDK_PATH environment variable is undefined! Please "
        "install PVCAM SDK first!"
    )
    quit()

# Locate correct library versions
if is_arch_x86_64:
    print("Machine is amd64")
    lib_dirs = [
        f"{pvcam_sdk_path}/Lib/amd64",
    ]
    libs = ["pvcam64"]
elif is_32bit:
    print("Machine is x86 32bit")
    lib_dirs = [
        f"{pvcam_sdk_path}/Lib/i386",
    ]
    libs = ["pvcam32"]

# Check for numpy install
try:
    import numpy

    numpy_path = numpy.get_include()
except ImportError:
    print(
        "\nERROR: numpy needs to be installed first. You can install it "
        "via:\n\n    $ pip install numpy"
    )
    quit()

include_dirs = [
    f"{pvcam_sdk_path}/inc/",
    numpy_path,
]

cpp_paths = glob("pyvcam/backend/*.cpp")
cpp_paths.append("pyvcam/pvcmodule.cpp")

# Define extension
print("\nBuilding pyvcam C++ extension...")
pyvcam_extension = Extension(
    "pyvcam.pvc",
    cpp_paths,
    include_dirs=include_dirs,
    library_dirs=lib_dirs,
    libraries=libs,
)

if __name__ == "__main__":
    setup(
        name=NAME,
        version=VERSION,
        description=DESCRIPTION,
        url=URL,
        packages=PACKAGES,
        ext_modules=[pyvcam_extension],
    )
