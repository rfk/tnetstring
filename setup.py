#
#  This is the tnetstring setuptools script.
#  Originally developed by Ryan Kelly, 2011.
#
#  This script is placed in the public domain.
#  If there's no public domain where you come from,
#  you can use it under the MIT license.
#

import sys
setup_kwds = {}
if sys.version_info > (3,):
    from setuptools import setup, Extension
    setup_kwds["test_suite"] = "tnetstring.test"
    setup_kwds["use_2to3"] = True
else:
    from distutils.core import setup, Extension


try:
    next = next
except NameError:
    def next(i):
        return i.next()


info = {}
try:
    src = open("tnetstring/__init__.py")
    lines = []
    ln = next(src)
    while "__version__" not in ln:
        lines.append(ln)
        ln = next(src)
    while "__version__" in ln:
        lines.append(ln)
        ln = next(src)
    exec("".join(lines),info)
except Exception:
    pass


NAME = "tnetstring"
VERSION = info["__version__"]
DESCRIPTION = "data serialization using typed netstrings"
LONG_DESC = info["__doc__"]
AUTHOR = "Ryan Kelly"
AUTHOR_EMAIL = "ryan@rfk.id.au"
URL="http://github.com/rfk/tnetstring"
LICENSE = "MIT"
KEYWORDS = "netstring serialize"
CLASSIFIERS = [
    "Programming Language :: Python",
    "Programming Language :: Python :: 2",
    #"Programming Language :: Python :: 3",
    "Development Status :: 4 - Beta",
    "License :: OSI Approved :: MIT License"
]

setup(name=NAME,
      version=VERSION,
      author=AUTHOR,
      author_email=AUTHOR_EMAIL,
      url=URL,
      description=DESCRIPTION,
      long_description=LONG_DESC,
      license=LICENSE,
      keywords=KEYWORDS,
      packages=["tnetstring","tnetstring.tests"],
      ext_modules = [
          Extension(name="_tnetstring",sources=["tnetstring/_tnetstring.c"]),
      ],
      classifiers=CLASSIFIERS,
      **setup_kwds
     )

