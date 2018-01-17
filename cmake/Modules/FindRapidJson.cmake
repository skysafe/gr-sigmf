# - Try to find RapidJson
# Once done, this will define
#
#  RAPIDJSON_FOUND - system has RapidJson
#  RAPIDJSON_INCLUDE_DIRS - the RapidJson include directories

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(RAPIDJSON_PKGCONF rapidjson)

# Main include dir
find_path(RAPIDJSON_INCLUDE_DIR
  NAMES rapidjson/rapidjson.h
  PATHS ${RAPIDJSON_PKGCONF_INCLUDE_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(RAPIDJSON_PROCESS_INCLUDES RAPIDJSON_INCLUDE_DIR)
libfind_process(RAPIDJSON)