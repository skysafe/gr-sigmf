# Copyright 2011,2012,2016 Free Software Foundation, Inc.
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
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

include_directories(${Boost_INCLUDE_DIR} ${RapidJson_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

find_package(UHD REQUIRED)
include_directories(${UHD_INCLUDE_DIRS})

# Tell boost time to use higher precision
add_definitions(-DBOOST_DATE_TIME_POSIX_TIME_STD_CONFIG)

list(APPEND sigmf_sources
    meta_namespace.cc
    nmea_parser.cc
    sink_impl.cc
    source_impl.cc
    sigmf_utils.cc
    annotation_sink_impl.cc
    writer_utils.cc
    reader_utils.cc
    usrp_gps_message_source_impl.cc
)

set(sigmf_sources "${sigmf_sources}" PARENT_SCOPE)
if(NOT sigmf_sources)
    MESSAGE(STATUS "No C++ sources... skipping lib/")
    return()
endif(NOT sigmf_sources)

add_library(gnuradio-sigmf SHARED ${sigmf_sources})
target_link_libraries(gnuradio-sigmf
    ${UHD_LIBRARIES}
    ${Boost_LIBRARIES}
    gnuradio::gnuradio-runtime
    gnuradio::gnuradio-uhd
    gnuradio::gnuradio-blocks
    )
set_target_properties(gnuradio-sigmf PROPERTIES DEFINE_SYMBOL "gnuradio_sigmf_EXPORTS")

if(APPLE)
    set_target_properties(gnuradio-sigmf PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
    )
endif(APPLE)

########################################################################
# Install built library files
########################################################################
include(GrMiscUtils)
GR_LIBRARY_FOO(gnuradio-sigmf RUNTIME_COMPONENT "sigmf_runtime" DEVEL_COMPONENT "sigmf_devel")

########################################################################
# Build and register unit test
########################################################################
include(GrTest)

include_directories(${CPPUNIT_INCLUDE_DIRS})

list(APPEND test_sigmf_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sigmf.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_sigmf.cc
)

add_executable(test-sigmf ${test_sigmf_sources})

target_link_libraries(
  test-sigmf
  gnuradio::gnuradio-runtime
  ${Boost_LIBRARIES}
  ${CPPUNIT_LIBRARIES}
  gnuradio-sigmf
)

GR_ADD_TEST(test_sigmf test-sigmf)

########################################################################
# Print summary
########################################################################
message(STATUS "Using install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Building for version: ${VERSION} / ${LIBVER}")

