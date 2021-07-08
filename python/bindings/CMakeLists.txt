# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT sigmf_sources)
    MESSAGE(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT sigmf_sources)

########################################################################
# Check for pygccxml
########################################################################
GR_PYTHON_CHECK_MODULE_RAW(
    "pygccxml"
    "import pygccxml"
    PYGCCXML_FOUND
    )

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND sigmf_python_files
    time_mode_python.cc
    nmea_parser_python.cc
    sink_python.cc
    source_python.cc
    annotation_sink_python.cc
    usrp_gps_message_source_python.cc
    python_bindings.cc)

GR_PYBIND_MAKE_OOT(sigmf 
   ../..
   gr::sigmf
   "${sigmf_python_files}")

install(TARGETS sigmf_python DESTINATION ${GR_PYTHON_DIR}/gr_sigmf COMPONENT pythonapi)
