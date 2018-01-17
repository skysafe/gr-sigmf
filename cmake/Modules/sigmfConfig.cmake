INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_SIGMF sigmf)

FIND_PATH(
    SIGMF_INCLUDE_DIRS
    NAMES sigmf/api.h
    HINTS $ENV{SIGMF_DIR}/include
        ${PC_SIGMF_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    SIGMF_LIBRARIES
    NAMES gnuradio-sigmf
    HINTS $ENV{SIGMF_DIR}/lib
        ${PC_SIGMF_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SIGMF DEFAULT_MSG SIGMF_LIBRARIES SIGMF_INCLUDE_DIRS)
MARK_AS_ADVANCED(SIGMF_LIBRARIES SIGMF_INCLUDE_DIRS)

