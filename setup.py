import os, platform
from setuptools import setup, find_packages
from setuptools.extension import Extension
import pip
import shutil

is_windows = 'win' in platform.system().lower()
is_linux = 'lin' in platform.system().lower()

print('Operating system: ' + platform.system())
print('Machine architecture: ' + platform.machine())

if not is_linux and not is_windows:
    print(' Operating systems other than Windows or Linux are not supported.')
    quit()

elif is_linux:
    is_arch_aarch64 = 'aarch64' in platform.machine().lower()
    is_arch_x86_64 = 'x86_64' in platform.machine().lower()
    is_arch_i686 = 'i686' in platform.machine().lower()

    if not is_arch_aarch64 and not is_arch_x86_64 and not is_arch_i686:
        print(' Machine architecture is not supported, it must be aarch64, x86_64 or i686.')
        quit()

elif is_windows:
    is_arch_x86_64 = 'amd64' in platform.machine().lower()
    is_32bit = 'x86' in platform.machine().lower()

    if not is_arch_x86_64 and not is_32bit:
        print(' Machine architecture is not supported, it must be amd64 or x86 32bit.')
        quit()

if is_linux:
    print('************************************************************\n')
    print('Pre-install necessary packages  \n')
    print('   sudo apt-get install python3-pip  \n')
    print('   sudo pip3 install numpy  \n')
    print('************************************************************\n')
    print('Build package:     sudo -E python3 setup.py build  \n')
    print('Install package:   sudo -E python3 setup.py install  \n')
    print('Create Wheel dist: sudo -E python3 setup.py sdist bdist_wheel  \n')
    print('Uninstall package: sudo pip3 uninstall pyvcam  \n')
    print('************************************************************\n')
elif is_windows:
    print('************************************************************\n')
    print('Pre-install necessary packages as admin  \n')
    print('   python -m pip install --upgrade pip setuptools wheel numpy  \n')
    print('************************************************************\n')
    print('Build package:     python setup.py build  \n')
    print('Install package:   python setup.py install  \n')
    print('Create Wheel dist: python setup.py sdist bdist_wheel  \n')
    print('Uninstall package: pip uninstall pyvcam  \n')
    print('************************************************************\n')

pvcam_sdk_path = os.environ['PVCAM_SDK_PATH']
import numpy
include_dirs = [numpy.get_include()]

if is_linux:
    extra_compile_args = ['-std=c++11']
    include_dirs.append('{}/include/'.format(pvcam_sdk_path))

    if is_arch_aarch64:
        lib_dirs = ['{}/library/aarch64'.format(pvcam_sdk_path)]
    elif is_arch_x86_64:
        lib_dirs = ['{}/library/x86_64'.format(pvcam_sdk_path)]
    elif is_arch_i686:
        lib_dirs = ['{}/library/i686'.format(pvcam_sdk_path)]

    libs = ['pvcam']

elif is_windows:
    extra_compile_args = []
    include_dirs.append('{}/inc/'.format(pvcam_sdk_path))

    if is_arch_x86_64:
        lib_dirs = ['{}/Lib/amd64'.format(pvcam_sdk_path)]
        libs = ['pvcam64']
    elif is_32bit:
        lib_dirs = ['{}/Lib/i386'.format(pvcam_sdk_path)]
        libs = ['pvcam32']

ext_modules = [Extension('pyvcam.pvc',
                         ['src/pyvcam/pvcmodule.cpp'],
                         extra_compile_args=extra_compile_args,
                         include_dirs=include_dirs,
                         library_dirs=lib_dirs,
                         libraries=libs)]

setup(name='pyvcam',
      version='2.1.5',
      author='Teledyne Photometrics',
      author_email='Steve.Bellinger@Teledyne.com',
      url='https://github.com/Photometrics/PyVCAM',
      description='Python wrapper for PVCAM functionality.',
      packages=['pyvcam'],
      package_dir={'pyvcam': 'src/pyvcam'},
      py_modules=['pyvcam.constants'],
      ext_modules=ext_modules)

# TODO add checks for if a package is already installed and if so don't install it, if it is installed and up to date give option to update or not
# pip.main(['install', 'wxPython'])
# pip.main(['install', 'pyserial'])
# pip.main(['install', 'opencv-python'])
# pip.main(['install', 'git+https://github.com/pearu/pylibtiff.git'])
# os.system('conda install -y -c conda-forge opencv=3.2.0')

print('\n\n*************** Finished ***************\n')
