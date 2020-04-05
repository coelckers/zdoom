# Generated by CMake

if("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS 2.5)
   message(FATAL_ERROR "CMake >= 2.6.0 required")
endif()
cmake_policy(PUSH)
cmake_policy(VERSION 2.6)
#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Protect against multiple inclusion, which would fail when already imported targets are added once more.
set(_targetsDefined)
set(_targetsNotDefined)
set(_expectedTargets)
foreach(_expectedTarget re2c lemon zipdir)
  list(APPEND _expectedTargets ${_expectedTarget})
  if(NOT TARGET ${_expectedTarget})
    list(APPEND _targetsNotDefined ${_expectedTarget})
  endif()
  if(TARGET ${_expectedTarget})
    list(APPEND _targetsDefined ${_expectedTarget})
  endif()
endforeach()
if("${_targetsDefined}" STREQUAL "${_expectedTargets}")
  unset(_targetsDefined)
  unset(_targetsNotDefined)
  unset(_expectedTargets)
  set(CMAKE_IMPORT_FILE_VERSION)
  cmake_policy(POP)
  return()
endif()
if(NOT "${_targetsDefined}" STREQUAL "")
  message(FATAL_ERROR "Some (but not all) targets in this export set were already defined.\nTargets Defined: ${_targetsDefined}\nTargets not yet defined: ${_targetsNotDefined}\n")
endif()
unset(_targetsDefined)
unset(_targetsNotDefined)
unset(_expectedTargets)


# Create imported target re2c
add_executable(re2c IMPORTED)

# Create imported target lemon
add_executable(lemon IMPORTED)

# Create imported target zipdir
add_executable(zipdir IMPORTED)

# Import target "re2c" for configuration "Debug"
set_property(TARGET re2c APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(re2c PROPERTIES
  IMPORTED_LOCATION_DEBUG "C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/tools/re2c/re2c.exe"
  )

# Import target "lemon" for configuration "Debug"
set_property(TARGET lemon APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(lemon PROPERTIES
  IMPORTED_LOCATION_DEBUG "C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/tools/lemon/lemon.exe"
  )

# Import target "zipdir" for configuration "Debug"
set_property(TARGET zipdir APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(zipdir PROPERTIES
  IMPORTED_LOCATION_DEBUG "C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/tools/zipdir/zipdir.exe"
  )

# This file does not depend on other imported targets which have
# been exported from the same project but in a separate export set.

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
cmake_policy(POP)
