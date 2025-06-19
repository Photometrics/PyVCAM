import os
import platform
import sys
from setuptools import setup, Extension
# from setuptools.extension import Extension
from setuptools.command.build_ext import build_ext as _build_ext


class BuildExt(_build_ext):
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
        # pylint: disable=import-outside-toplevel
        import numpy
        include_dirs.append(numpy.get_include())


# print(f'Operating system: {platform.system()}')
# print(f'Machine architecture: {platform.machine()}')

is_windows = 'win' in platform.system().lower()
is_linux = 'lin' in platform.system().lower()

current_arch = platform.machine().lower()
supported_archs = []

if is_linux:
    supported_archs = ['x86_64', 'i686', 'aarch64']
elif is_windows:
    supported_archs = ['amd64', 'x86']
else:
    sys.exit('Operating systems other than Windows or Linux are not supported.')
if current_arch not in supported_archs:
    sys.exit(f'Machine architecture is not supported, it must be one of {supported_archs}.')

pvcam_sdk_path = os.environ['PVCAM_SDK_PATH']
include_dirs = []
library_dirs = []
libraries = []
extra_compile_args = []
sources = []
depends = []

if is_linux:
    include_dirs.append(f'{pvcam_sdk_path}/include')
    library_dirs.append(f'{pvcam_sdk_path}/library/{current_arch}')
    libraries.append('pvcam')
    extra_compile_args.append('-std=c++14')

elif is_windows:
    include_dirs.append(f'{pvcam_sdk_path}/inc')
    if current_arch == 'x86':
        library_dirs.append(f'{pvcam_sdk_path}/lib/i386')
        libraries.append('pvcam32')
    else:
        library_dirs.append(f'{pvcam_sdk_path}/lib/{current_arch}')
        libraries.append('pvcam64')

include_dirs.append('src/pyvcam')
sources.append('src/pyvcam/pvcmodule.cpp')
# depends.append('src/pyvcam/some_header.h')

ext_modules = [
    Extension(
        name='pyvcam.pvc',
        sources=sources,
        depends=depends,
        include_dirs=include_dirs,
        library_dirs=library_dirs,
        libraries=libraries,
        extra_compile_args=extra_compile_args,
    )
]

setup(
    cmdclass={'build_ext': BuildExt},
    setup_requires=['numpy'],
    ext_modules=ext_modules,

    # Workaround: Home page doesn't show up with 'pip show' when set in pyproject.toml
    url='https://github.com/Photometrics/PyVCAM',

    # Everything else set in pyproject.toml
)
