import os, platform
from setuptools import setup
from setuptools.extension import Extension
from setuptools.command.build_ext import build_ext as _build_ext

is_windows = 'win' in platform.system().lower()
is_linux = 'lin' in platform.system().lower()

#print('Operating system: ' + platform.system())
#print('Machine architecture: ' + platform.machine())

if not is_linux and not is_windows:
    print('Operating systems other than Windows or Linux are not supported.')
    quit()

elif is_linux:
    is_arch_aarch64 = 'aarch64' in platform.machine().lower()
    is_arch_x86_64 = 'x86_64' in platform.machine().lower()
    is_arch_i686 = 'i686' in platform.machine().lower()

    if not is_arch_aarch64 and not is_arch_x86_64 and not is_arch_i686:
        print('Machine architecture is not supported, it must be aarch64, x86_64 or i686.')
        quit()

elif is_windows:
    is_arch_x86_64 = 'amd64' in platform.machine().lower()
    is_32bit = 'x86' in platform.machine().lower()

    if not is_arch_x86_64 and not is_32bit:
        print('Machine architecture is not supported, it must be amd64 or x86 32bit.')
        quit()

pvcam_sdk_path = os.environ['PVCAM_SDK_PATH']
include_dirs = []
lib_dirs = []
libs = []
extra_compile_args = []
sources = []

class build_ext(_build_ext):
    def finalize_options(self) -> None:
        # Prevent numpy from thinking it is still in its setup process
        # https://github.com/singingwolfboy/nose-progressive/commit/d8dce093910724beff86081327565b11ea28dfbc#diff-fa26a70e79ad69caf5b4f938fcd73179afaf4c05229b19acb326ab89e4759fbfR25
        def _set_builtin(name, value):
            if isinstance(__builtins__, dict):
                __builtins__[name] = value
            else:
                setattr(__builtins__, name, value)

        _build_ext.finalize_options(self)
        _set_builtin('__NUMPY_SETUP__', False)
        import numpy
        include_dirs.append(numpy.get_include())

if is_linux:
    extra_compile_args.append('-std=c++11')
    include_dirs.append('{}/include'.format(pvcam_sdk_path))
    if is_arch_aarch64:
        lib_dirs.append('{}/library/aarch64'.format(pvcam_sdk_path))
    elif is_arch_x86_64:
        lib_dirs.append('{}/library/x86_64'.format(pvcam_sdk_path))
    elif is_arch_i686:
        lib_dirs.append('{}/library/i686'.format(pvcam_sdk_path))
    libs.append('pvcam')

elif is_windows:
    include_dirs.append('{}/inc'.format(pvcam_sdk_path))
    if is_arch_x86_64:
        lib_dirs.append('{}/Lib/amd64'.format(pvcam_sdk_path))
        libs.append('pvcam64')
    elif is_32bit:
        lib_dirs.append('{}/Lib/i386'.format(pvcam_sdk_path))
        libs.append('pvcam32')

include_dirs.append('src/pyvcam')
sources.append('src/pyvcam/pvcmodule.cpp')

ext_modules = [Extension('pyvcam.pvc',
                         sources=sources,
                         extra_compile_args=extra_compile_args,
                         include_dirs=include_dirs,
                         library_dirs=lib_dirs,
                         libraries=libs)]

setup(name='pyvcam',
      version='2.1.9',
      author='Teledyne Photometrics',
      author_email='Steve.Bellinger@Teledyne.com',
      url='https://github.com/Photometrics/PyVCAM',
      description='Python wrapper for PVCAM functionality.',
      packages=['pyvcam'],
      package_dir={'pyvcam': 'src/pyvcam'},
      py_modules=['pyvcam.constants'],
      cmdclass={'build_ext': build_ext},
      setup_requires=['numpy'],
      install_requires=['numpy'],
      ext_modules=ext_modules)
