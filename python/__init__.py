#
# Copyright 2008,2009 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# The presence of this file turns this directory into a Python package

'''
This is the GNU Radio SIGMF module. Place your Python package
description here (python/__init__.py).
'''
import os

from gr_sigmf.sigmf_python import *
from gr_sigmf import *

# import any pure python here
#

annotation_mode_clear = annotation_mode.clear
annotation_mode_keep = annotation_mode.keep

sigmf_time_mode_absolute = sigmf_time_mode.absolute
sigmf_time_mode_relative = sigmf_time_mode.relative
