from distutils.core import setup, Extension
setup(name = 'ROI', version = '1.0',  \
   ext_modules = [Extension('ROI', ['hooks.c'])])
