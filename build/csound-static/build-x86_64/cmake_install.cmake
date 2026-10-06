# Install script for directory: /Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/include/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/alessandropetrolati/Library/Frameworks" TYPE DIRECTORY FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CsoundLib64.framework" USE_SOURCE_PERMISSIONS)
  if(EXISTS "$ENV{DESTDIR}/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0/CsoundLib64" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0/CsoundLib64")
    execute_process(COMMAND /usr/bin/install_name_tool
      -add_rpath "/Users/alessandropetrolati/Library/Frameworks"
      "$ENV{DESTDIR}/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0/CsoundLib64")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" -x "$ENV{DESTDIR}/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0/CsoundLib64")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/libCsoundLib64.a")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib" TYPE STATIC_LIBRARY FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/libCsoundLib64.a")
  if(EXISTS "$ENV{DESTDIR}/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/libCsoundLib64.a" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/libCsoundLib64.a")
    execute_process(COMMAND "/usr/bin/ranlib" "$ENV{DESTDIR}/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/libCsoundLib64.a")
  endif()
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/Opcodes/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/InOut/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/Frontends/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/languages/python/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/platform/bela/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/util/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/util1/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/po/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/tests/c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/tests/commandline/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/tests/regression/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/tests/soak/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/tests/python/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundTargets.cmake"
         "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CMakeFiles/Export/bdcb8957b056fd952b2019af13dc8ccf/CsoundTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CMakeFiles/Export/bdcb8957b056fd952b2019af13dc8ccf/CsoundTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CMakeFiles/Export/bdcb8957b056fd952b2019af13dc8ccf/CsoundTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundStaticTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundStaticTargets.cmake"
         "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CMakeFiles/Export/bdcb8957b056fd952b2019af13dc8ccf/CsoundStaticTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundStaticTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/csound/CsoundStaticTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CMakeFiles/Export/bdcb8957b056fd952b2019af13dc8ccf/CsoundStaticTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CMakeFiles/Export/bdcb8957b056fd952b2019af13dc8ccf/CsoundStaticTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CsoundConfig.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/CsoundConfigVersion.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/cmake/FindCsound.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/csound" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/cmake/Modules/FindWasmtime.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/pkgconfig/csound.pc")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-install/x86_64/lib/pkgconfig" TYPE FILE FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/csound.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0/samples")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/Users/alessandropetrolati/Library/Frameworks/CsoundLib64.framework/Versions/7.0" TYPE DIRECTORY FILES "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/src/samples")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/build/csound-static/build-x86_64/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
