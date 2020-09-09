# Copyright 2011 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Include python install macros
########################################################################
include(GrPython)
if(NOT PYTHONINTERP_FOUND)
    return()
endif()

########################################################################
# Install python sources
########################################################################
GR_PYTHON_INSTALL(
    FILES
    __init__.py
    DESTINATION ${GR_PYTHON_DIR}/gr_sigmf
)

########################################################################
# Handle the unit tests
########################################################################
include(GrTest)

set(GR_TEST_TARGET_DEPS gnuradio-sigmf)
set(GR_TEST_PYTHON_DIRS ${CMAKE_BINARY_DIR}/swig)

GR_ADD_TEST(qa_annotation_sink ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_annotation_sink.py)
GR_ADD_TEST(qa_archive ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_archive.py)
GR_ADD_TEST(qa_hash ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_hash.py)
GR_ADD_TEST(qa_sink ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_sink.py)
GR_ADD_TEST(qa_source ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_source.py)
GR_ADD_TEST(qa_source_to_sink ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_source_to_sink.py)
GR_ADD_TEST(qa_type_converter ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_type_converter.py)
GR_ADD_TEST(qa_crop ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_crop.py)
GR_ADD_TEST(qa_nmea_parser ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qa_nmea_parser.py)
