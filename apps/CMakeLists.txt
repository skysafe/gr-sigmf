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

# include(GrPython)

########################################################################
# Dependencies
########################################################################

## Boost
# if(WIN32)
#     set(Boost_USE_STATIC_LIBS ON)
#     add_definitions("-DBOOST_ALL_NO_LIB")
# endif()

find_package(Boost
#   1.61
  REQUIRED
  COMPONENTS system program_options filesystem log)

# # link_libraries(${Boost_LIBRARIES})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${RapidJson_INCLUDE_DIR})

# # ## Threading
# # find_package(Threads REQUIRED)
# # link_libraries(${CMAKE_THREAD_LIBS_INIT})

# ## UHD
find_package(UHD REQUIRED)
include_directories(${UHD_INCLUDE_DIRS})

# ## Gnuradio
# find_package(Gnuradio "3.7.2" REQUIRED)
# include_directories(${GNURADIO_ALL_INCLUDE_DIRS})

####################################################
############### sigmf-record #######################
####################################################

set(SIGMF_RECORD_SRCFILES
    "sigmf_record.cc"
)


add_executable(sigmf-record
    ${SIGMF_RECORD_SRCFILES}
)

# get_cmake_property(_variableNames VARIABLES)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

target_link_libraries(sigmf-record 
    ${UHD_LIBRARIES}
    ${Boost_LIBRARIES}
    ${GNURADIO_ALL_LIBRARIES}
    gnuradio-sigmf
    )

install(TARGETS sigmf-record DESTINATION bin)

####################################################
############### sigmf-play #########################
####################################################

set(SIGMF_PLAY_SRCFILES
    "sigmf_play.cc"
)

add_executable(sigmf-play
    ${SIGMF_PLAY_SRCFILES}
)

target_link_libraries(sigmf-play 
    ${UHD_LIBRARIES}
    ${Boost_LIBRARIES}
    ${GNURADIO_ALL_LIBRARIES}
    gnuradio-sigmf
    )

install(TARGETS sigmf-play DESTINATION bin)

####################################################
############### Python-based apps ##################
####################################################
GR_PYTHON_INSTALL(
    PROGRAMS
    sigmf_archive.py
    sigmf_hash.py
    DESTINATION ${GR_RUNTIME_DIR}
    COMPONENT "sigmf_python"
)