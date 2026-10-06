
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was CsoundConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# CsoundConfig.cmake - CMake package configuration file for Csound
#
# This file is part of the Csound CMake package configuration.
# It provides a stable Csound::Csound imported target for consumers,
# backed by the exported platform-specific shared library target
# (typically Csound::CsoundLib64 or Csound::CsoundLib).
#
# When the static library is installed (BUILD_STATIC_LIBRARY=ON),
# a Csound::Csound-static target is also available.
#
# Usage:
#   find_package(Csound CONFIG)
#   target_link_libraries(myapp PRIVATE Csound::Csound)
#
# For static linking (requires static library to be installed):
#   target_link_libraries(myapp PRIVATE Csound::Csound-static)
#
# Or for version-constrained searches:
#   find_package(Csound 7.0 CONFIG)
#
# If Csound is installed in a non-standard location, set Csound_DIR:
#   cmake -DCsound_DIR=/path/to/csound/share/csound ..
#
# On macOS with the framework installer:
#   cmake -DCsound_DIR=/Applications/Csound/CsoundLib64.framework/Versions/7.0/Resources/cmake ..
#
# On Windows with the default installer:
#   cmake -DCsound_DIR="C:/Program Files/Csound7/cmake" ..

set(_csound_shared_target "")
set(_csound_framework_version_dir "")
set(_csound_framework_library_name "")

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../CsoundLib64")
	set(_csound_framework_version_dir "${CMAKE_CURRENT_LIST_DIR}/../..")
	set(_csound_framework_library_name CsoundLib64)
	set(_csound_shared_target Csound::CsoundLib64)
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../CsoundLib")
	set(_csound_framework_version_dir "${CMAKE_CURRENT_LIST_DIR}/../..")
	set(_csound_framework_library_name CsoundLib)
	set(_csound_shared_target Csound::CsoundLib)
else()
	include("${CMAKE_CURRENT_LIST_DIR}/CsoundTargets.cmake")

	if(TARGET Csound::CsoundLib64)
		set(_csound_shared_target Csound::CsoundLib64)
	elseif(TARGET Csound::CsoundLib)
		set(_csound_shared_target Csound::CsoundLib)
	endif()
endif()

if(NOT _csound_framework_version_dir STREQUAL "")
	if(NOT TARGET ${_csound_shared_target})
		add_library(${_csound_shared_target} SHARED IMPORTED)
		set_target_properties(${_csound_shared_target} PROPERTIES
			FRAMEWORK 1
			IMPORTED_LOCATION "${_csound_framework_version_dir}/${_csound_framework_library_name}"
			INTERFACE_INCLUDE_DIRECTORIES "${_csound_framework_version_dir}/Headers"
		)
	endif()
endif()

if(NOT TARGET Csound::Csound AND NOT _csound_shared_target STREQUAL "")
	add_library(Csound::Csound INTERFACE IMPORTED)
	set_target_properties(Csound::Csound PROPERTIES
		INTERFACE_LINK_LIBRARIES "${_csound_shared_target}"
	)
endif()

# Optionally load static library targets when available
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/CsoundStaticTargets.cmake")
	include(CMakeFindDependencyMacro)
	if(ON)
		find_dependency(SndFile CONFIG)
	endif()
	if(TRUE)
		find_dependency(SampleRate CONFIG)
	endif()
	if(ON)
		find_dependency(Threads)
	endif()
	if(0)
		find_dependency(Intl)
	endif()
	include("${CMAKE_CURRENT_LIST_DIR}/CsoundStaticTargets.cmake")

	set(_csound_static_target "")
	if(TARGET Csound::CsoundLib64-static)
		set(_csound_static_target Csound::CsoundLib64-static)
	elseif(TARGET Csound::CsoundLib-static)
		set(_csound_static_target Csound::CsoundLib-static)
	endif()

	if(NOT TARGET Csound::Csound-static AND NOT _csound_static_target STREQUAL "")
		add_library(Csound::Csound-static INTERFACE IMPORTED)
		set_target_properties(Csound::Csound-static PROPERTIES
			INTERFACE_LINK_LIBRARIES "${_csound_static_target}"
		)
	endif()
	unset(_csound_static_target)
endif()

unset(_csound_framework_version_dir)
unset(_csound_framework_library_name)
unset(_csound_shared_target)

check_required_components(Csound)
