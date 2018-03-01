# - Try to find RapidJson
# Once done, this will define
#
#  RAPIDJSON_FOUND - system has RapidJson
#  RAPIDJSON_INCLUDE_DIRS - the RapidJson include directories

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(RapidJson_PKGCONF RapidJSON)

# Main include dir
find_path(RapidJson_INCLUDE_DIR
  NAMES rapidjson/rapidjson.h
  PATHS ${RAPIDJSON_PKGCONF_INCLUDE_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(RapidJson_PROCESS_INCLUDES RapidJson_INCLUDE_DIR)
libfind_process(RapidJson)
