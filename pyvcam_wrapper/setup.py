import os,platform
from setuptools import setup, find_packages
from setuptools.extension import Extension
import pip
import shutil

is_windows=(platform.system().lower().find("win") > -1)
is_linux=(platform.system().lower().find("lin") > -1)

is_32bit= ''

if is_linux:
   is_arch_x86_64 =  (platform.machine().lower().find("x86_64") > -1)
   is_arch_i686 =  (platform.machine().lower().find("i686") > -1)
   is_arch_aarch64 =  (platform.machine().lower().find("aarch64") > -1)
elif is_windows:
   is_arch_x86_64 =  (platform.machine().lower().find("amd64") > -1)
   is_32bit =  (platform.machine().lower().find("x86") > -1)

if is_linux:
   print(' os is Linux')
elif is_windows:
   print(' os is Windows')
else:
   print(' Operating systems other than Windows or Linux is not supported!')
   quit()

if is_linux:
   if is_arch_x86_64:
       print(' Machine architecture is x86_64')
   elif is_arch_i686:
       print(' Machine architecture is i686')
   elif is_arch_aarch64:
       print(' Machine architecture is aarch64')
   else:
       print(' The machine architecture are not supported, it must be x86_64, i686 or aarch64.')
       quit()
elif is_windows:
    if is_arch_x86_64:
        print(' Machine is amd64')
    elif is_32bit:
        print(' Machine is x86 32bit')
    else:
       print(' This machine architecture is not supported, it must be amd64 or x86 32bit.')
       quit()

if is_linux:
    print('************************************************************\n')
    print('Preinstall necessary packages  \n')
    print('   sudo apt-get install python3-pip  \n')
    print('   sudo pip3 install numpy  \n')
    print('************************************************************\n')
    print('Using "sudo python3 setup.py build"  to build this package \n')
    print('Using "sudo python3 setup.py install"  to install this package \n')
    print('Using "sudo pip3 uninstall pyvcam"  to uninstall this package \n')
    print('************************************************************\n')
elif is_windows:
    print('************************************************************\n')
    print('Preinstall necessary packages  \n')
    print('   python3 -m pip install --upgrade pip setuptools wheel  \n')
    print('   pip install "numpy-1.15.0+mkl-cp37-cp37m-win_amd64.whl"  \n')
    print('************************************************************\n')
    print('Using "python3 setup.py build"  to build this package \n')
    print('Using "python3 setup.py install"  to install this package \n')
    print('Using "pip3 uninstall pyvcam"  to uninstall this package \n')
    print('************************************************************\n')

import numpy
pvcam_sdk_path = ''
packages = ['pyvcam']
package_dir = {'pyvcam': 'src/pyvcam'}
py_modules = ['pyvcam.constants']
libs = ['pvcam']
include_dirs = ''
lib_dirs = ''
ext_modules = ''
if is_linux:
    pvcam_sdk_path = '/opt/pvcam/sdk'
    include_dirs = ["{}/include/".format(pvcam_sdk_path),
                         numpy.get_include()]    
    if is_arch_x86_64:
        lib_dirs = ['{}/library/x86_64'.format(pvcam_sdk_path)]
    elif is_arch_i686:
        lib_dirs = ['{}/library/i686'.format(pvcam_sdk_path)]
    elif is_arch_aarch64:
        lib_dirs = ['{}/library/aarch64'.format(pvcam_sdk_path)]

    ext_modules = [Extension('pyvcam.pvc',
                         ['src/pyvcam/pvcmodule.cpp'],
                         extra_compile_args=['-std=c++11'],
                         include_dirs=include_dirs,
                         library_dirs=lib_dirs,
                         libraries=libs)]
elif is_windows:
    pvcam_sdk_path = os.environ["PVCAM_SDK_PATH"]
    include_dirs = ["{}/inc/".format(pvcam_sdk_path),
                         numpy.get_include()]
    if is_arch_x86_64:
        lib_dirs = ['{}/Lib/amd64'.format(pvcam_sdk_path)]
        libs = ['pvcam64']
    elif is_32bit:
        lib_dirs = ['{}/Lib/i386'.format(pvcam_sdk_path)]
        libs = ['pvcam32']
    ext_modules = [Extension('pyvcam.pvc',
                         ['src/pyvcam/pvcmodule.cpp'],
                         include_dirs=include_dirs,
                         library_dirs=lib_dirs,
                         libraries=libs)]
setup(name='pyvcam',
      version='1.1',
      description='Python wrapper for PVCAM functionality.',
      packages=packages,
      package_dir=package_dir,
      py_modules=py_modules,
      ext_modules=ext_modules)
print('\nPyVCAM built, removing temp directories\n\n')

#TODO add checks for if a package is already installed and if so don't install it, if it is installed and up to date give option to update or not
#pip.main(['install', 'wxPython'])
#pip.main(['install', 'pyserial'])
#pip.main(['install', 'opencv-python'])
#pip.main(['install', 'git+https://github.com/pearu/pylibtiff.git'])
#os.system('conda install -y -c conda-forge opencv=3.2.0')
shutil.rmtree('build', ignore_errors=True, onerror=None)
shutil.rmtree('dist', ignore_errors=True, onerror=None)
shutil.rmtree('pyvcam.egg-info', ignore_errors=True, onerror=None)

print('\n\n*************** Finished Installing PyVCAM ***************\n')
