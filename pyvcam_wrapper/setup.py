import os
import numpy
from setuptools import setup, find_packages
from setuptools.extension import Extension
import pip
import shutil

pvcam_sdk_path = os.environ["PVCAM_SDK_PATH"]

packages = ['pyvcam']
package_dir = {'pyvcam': 'src/pyvcam'}
py_modules = ['pyvcam.constants']
include_dirs = ["{}/inc/".format(pvcam_sdk_path),
                numpy.get_include()]
lib_dirs = ['{}/Lib/amd64'.format(pvcam_sdk_path)]
libs = ['pvcam64']
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
print('\nPyVCAM installed, removing temp directories\n\n')

#TODO add checks for if a package is already installed and if so don't install it, if it is installed and up to date give option to update or not
#pip.main(['install', 'wxPython'])
#pip.main(['install', 'pyserial'])
#pip.main(['install', 'opencv-python'])
#pip.main(['install', 'git+https://github.com/pearu/pylibtiff.git'])
#os.system('conda install -y -c conda-forge opencv=3.2.0')
shutil.rmtree('build')
shutil.rmtree('dist')
shutil.rmtree('pyvcam.egg-info')

print('\n\n*************** Finished Installing PyVCAM ***************\n')
