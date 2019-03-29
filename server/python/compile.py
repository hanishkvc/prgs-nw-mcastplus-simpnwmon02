
from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

ext_modules = [
    Extension("network",  ["network.py"]),
    Extension("context",  ["context.py"]),
    Extension("status",  ["status.py"]),
    Extension("mcast",  ["mcast.py"]),
#   ... all your modules that need be compiled ...
]

setup(
    name = 'SimpNwMon02',
    cmdclass = {'build_ext': build_ext},
    ext_modules = ext_modules
)

